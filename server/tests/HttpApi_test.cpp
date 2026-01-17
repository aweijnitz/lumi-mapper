#include "ServerApp.h"
#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <memory>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace projection::server {
namespace {

int reservePort() {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sock >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    REQUIRE(::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    int port = ntohs(addr.sin_port);
    ::close(sock);
    return port;
}

std::string tempDbPath(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path.string();
}

std::filesystem::path tempWebRoot(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path / "assets");
    return path;
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::out | std::ios::binary);
    REQUIRE(file);
    file << contents;
}

struct TestServerContext {
    db::SqliteConnection connection;
    repo::FeedRepository feedRepo;
    repo::SceneRepository sceneRepo;
    repo::CueRepository cueRepo;
    repo::ProjectRepository projectRepo;
    http::HttpServer httpServer;

    explicit TestServerContext(const std::string& dbPath, std::string webRoot = "")
        : feedRepo(connection),
          sceneRepo(connection),
          cueRepo(connection),
          projectRepo(connection),
          httpServer(feedRepo, sceneRepo, cueRepo, projectRepo, nullptr, true, std::move(webRoot)) {
        connection.open(dbPath);
        db::SchemaMigrations::applyMigrations(connection);
    }
};

class ServerRunner {
public:
    ServerRunner(http::HttpServer& server, int port) : server_(server) {
        thread_ = std::thread([this, port] {
            try {
                server_.start(port);
            } catch (...) {
                startError_ = std::current_exception();
            }
        });
    }

    ~ServerRunner() {
        if (thread_.joinable()) {
            server_.stop();
            thread_.join();
        }
    }

    bool hadError() const { return static_cast<bool>(startError_); }
    std::exception_ptr error() const { return startError_; }

private:
    http::HttpServer& server_;
    std::thread thread_;
    std::exception_ptr startError_;
};

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);  // 200ms
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

bool waitForServer(httplib::Client& client, http::HttpServer& server, const ServerRunner& runner) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (runner.hadError()) {
            std::rethrow_exception(runner.error());
        }
        if (server.isRunning()) {
            if (auto res = client.Get("/api/projects")) {
                (void)res;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

std::string feedBody(const std::string& projectId) {
    nlohmann::json feed{{"projectId", projectId},
                        {"id", "1"},
                        {"name", "Camera"},
                        {"type", "Camera"},
                        {"configJson", "{}"}};
    return feed.dump();
}

std::string sceneBody(const std::string& projectId) {
    nlohmann::json scene{{"projectId", projectId},
                         {"id", "1"},
                         {"name", "Main"},
                         {"description", "Test scene"},
                         {"surfaces", nlohmann::json::array()}};
    return scene.dump();
}

std::string sceneWithSurfaceBody(const std::string& projectId, const std::string& sceneId, const std::string& feedId) {
    nlohmann::json scene{
        {"projectId", projectId},
        {"id", sceneId},
        {"name", "Main"},
        {"description", "With surface"},
        {"surfaces",
         {{{"id", "s1"},
           {"name", "surf"},
           {"vertices", {{{"x", 0}, {"y", 0}}, {{"x", 1}, {"y", 0}}, {{"x", 0}, {"y", 1}}}},
           {"feedId", feedId},
           {"opacity", 1.0},
           {"brightness", 1.0},
           {"blendMode", "Normal"},
           {"zOrder", 0}}}}};
    return scene.dump();
}

std::string cueBody(const std::string& projectId, const std::string& cueId, const std::string& sceneId,
                    const std::string& surfaceId) {
    nlohmann::json cue{{"projectId", projectId},
                       {"id", cueId},
                       {"name", "CueName"},
                       {"sceneId", sceneId},
                       {"surfaceOpacities", {{{"surfaceId", surfaceId}, {"value", 1.0}}}},
                       {"surfaceBrightnesses", {{{"surfaceId", surfaceId}, {"value", 1.0}}}}};
    return cue.dump();
}

std::string projectBody(const std::string& projectId, const std::vector<std::string>& cueOrder,
                        nlohmann::json settings = nlohmann::json::object()) {
    auto normalizedSettings = settings.empty() ? nlohmann::json::object() : settings;
    if (!normalizedSettings.contains("globalConfig") || normalizedSettings["globalConfig"].is_null()) {
        normalizedSettings["globalConfig"] = nlohmann::json::object();
    }
    nlohmann::json project{{"id", projectId},
                           {"name", "ProjectName"},
                           {"description", "Project description"},
                           {"cueOrder", cueOrder},
                           {"settings", normalizedSettings}};
    return project.dump();
}

void createProject(httplib::Client& client, const std::string& projectId) {
    auto res = client.Post("/api/projects", projectBody(projectId, {}), "application/json");
    REQUIRE(res != nullptr);
    if (res->status != 201) {
        throw std::runtime_error("Project create failed with status " + std::to_string(res->status) +
                                 " body: " + res->body);
    }
}

}  // namespace

TEST_CASE("HTTP API can create and list feeds", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_feeds.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);

    auto postRes = client->Post(("/api/projects/" + projectId + "/feeds").c_str(),
                                feedBody(projectId), "application/json");
    REQUIRE(postRes != nullptr);
    if (postRes->status != 201) {
        throw std::runtime_error("Feed POST failed with status " + std::to_string(postRes->status) +
                                 " body: " + postRes->body);
    }

    auto getRes = client->Get(("/api/projects/" + projectId + "/feeds").c_str());
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);

    auto bodyJson = nlohmann::json::parse(getRes->body);
    REQUIRE(bodyJson.is_array());
    REQUIRE(bodyJson.size() == 1);
    REQUIRE(bodyJson[0]["id"] == "1");

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API can create and list scenes", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_scenes.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);

    auto postRes = client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                                sceneBody(projectId), "application/json");
    REQUIRE(postRes != nullptr);
    if (postRes->status != 201) {
        throw std::runtime_error("Scene POST failed with status " + std::to_string(postRes->status) +
                                 " body: " + postRes->body);
    }

    auto getRes = client->Get(("/api/projects/" + projectId + "/scenes").c_str());
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);

    auto bodyJson = nlohmann::json::parse(getRes->body);
    REQUIRE(bodyJson.is_array());
    REQUIRE(bodyJson.size() == 1);
    REQUIRE(bodyJson[0]["id"] == "1");

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API can create, fetch, update, and delete projects", "[http][integration][projects]") {
    auto dbPath = tempDbPath("http_api_projects.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                         "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                         sceneWithSurfaceBody(projectId, "scene-1", "1"), "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/cues").c_str(),
                         cueBody(projectId, "cue-1", "scene-1", "s1"), "application/json")->status == 201);

    nlohmann::json settings{{"controllers", {{"fader1", "master"}}}, {"midiChannels", {1, 2}}, {"globalConfig", {}}};
    auto payload = projectBody(projectId, {"cue-1"}, settings);
    auto updateRes = client->Put(("/api/projects/" + projectId).c_str(), payload, "application/json");
    REQUIRE(updateRes != nullptr);
    if (updateRes->status != 200) {
        throw std::runtime_error("Project update failed with status " + std::to_string(updateRes->status) +
                                 " body: " + updateRes->body + " payload: " + payload);
    }

    auto listRes = client->Get("/api/projects");
    REQUIRE(listRes != nullptr);
    REQUIRE(listRes->status == 200);
    auto projects = nlohmann::json::parse(listRes->body);
    REQUIRE(projects.is_array());
    REQUIRE(projects.size() == 1);
    REQUIRE(projects[0]["cueOrder"].size() == 1);

    auto getRes = client->Get(("/api/projects/" + projectId).c_str());
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);
    auto projectJson = nlohmann::json::parse(getRes->body);
    REQUIRE(projectJson["settings"]["controllers"]["fader1"] == "master");

    nlohmann::json updatedSettings{{"controllers", {{"knob1", "hue"}}}, {"midiChannels", {3}}, {"globalConfig", {}}};
    auto updatePayload = nlohmann::json{{"id", "ignored"},
                                        {"name", "ProjectName"},
                                        {"description", "Updated"},
                                        {"cueOrder", nlohmann::json::array({"cue-1"})},
                                        {"settings", updatedSettings}};
    auto updateRes2 = client->Put(("/api/projects/" + projectId).c_str(), updatePayload.dump(), "application/json");
    REQUIRE(updateRes2 != nullptr);
    REQUIRE(updateRes2->status == 200);
    auto updatedJson = nlohmann::json::parse(updateRes2->body);
    REQUIRE(updatedJson["description"] == "Updated");
    REQUIRE(updatedJson["settings"]["controllers"]["knob1"] == "hue");

    auto deleteRes = client->Delete(("/api/projects/" + projectId).c_str());
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 204);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API handles concurrent project list and create", "[http][integration][projects][concurrency]") {
    auto dbPath = tempDbPath("http_api_projects_concurrent.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);
    auto listClient = makeClient(port);
    auto createClient = makeClient(port);
    REQUIRE(waitForServer(*listClient, ctx.httpServer, runner));

    std::atomic<bool> listOk{true};
    std::atomic<bool> createOk{true};

    std::thread listThread([&] {
        for (int i = 0; i < 25; ++i) {
            auto res = listClient->Get("/api/projects");
            if (!res || res->status != 200) {
                listOk = false;
                return;
            }
        }
    });

    std::thread createThread([&] {
        for (int i = 0; i < 10; ++i) {
            const std::string projectId = "project-concurrent-" + std::to_string(i);
            auto res = createClient->Post("/api/projects", projectBody(projectId, {}), "application/json");
            if (!res || res->status != 201) {
                createOk = false;
                return;
            }
        }
    });

    listThread.join();
    createThread.join();

    REQUIRE(listOk);
    REQUIRE(createOk);
}

TEST_CASE("HTTP API rejects projects referencing unknown cues", "[http][integration][projects][validation]") {
    auto dbPath = tempDbPath("http_api_projects_validation.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto createRes = client->Post("/api/projects", projectBody("project-1", {"missing-cue"}), "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 400);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API returns 400 on invalid JSON", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_invalid_json.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);

    auto postRes = client->Post(("/api/projects/" + projectId + "/feeds").c_str(), "not-json", "application/json");
    REQUIRE(postRes != nullptr);
    REQUIRE(postRes->status == 400);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API validates required fields", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_missing_fields.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));
    const std::string projectId = "project-1";
    createProject(*client, projectId);
    nlohmann::json badFeed{{"name", "Missing id"}};
    auto postRes =
        client->Post(("/api/projects/" + projectId + "/feeds").c_str(), badFeed.dump(), "application/json");
    REQUIRE(postRes != nullptr);
    REQUIRE(postRes->status == 400);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API prevents deleting feeds referenced by scenes", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_feed_delete_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                         "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                         sceneWithSurfaceBody(projectId, "scene-guard", "1"), "application/json")->status == 201);

    auto delRes = client->Delete(("/api/projects/" + projectId + "/feeds/1").c_str());
    REQUIRE(delRes != nullptr);
    REQUIRE(delRes->status == 400);
    REQUIRE(delRes->body.find("referenced by scene") != std::string::npos);
    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API prevents deleting scenes referenced by cues", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_scene_delete_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                         "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                         sceneWithSurfaceBody(projectId, "scene-guard", "1"), "application/json")->status == 201);
    auto cueRes =
        client->Post(("/api/projects/" + projectId + "/cues").c_str(),
                     cueBody(projectId, "cue-1", "scene-guard", "s1"), "application/json");
    REQUIRE(cueRes != nullptr);
    REQUIRE(cueRes->status == 201);

    auto delRes = client->Delete(("/api/projects/" + projectId + "/scenes/scene-guard").c_str());
    REQUIRE(delRes != nullptr);
    REQUIRE(delRes->status == 400);
    REQUIRE(delRes->body.find("referenced by cue") != std::string::npos);
    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API supports cue CRUD", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_cues_crud.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                         "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                         sceneWithSurfaceBody(projectId, "scene-1", "1"), "application/json")->status == 201);

    auto createRes =
        client->Post(("/api/projects/" + projectId + "/cues").c_str(),
                     cueBody(projectId, "cue-1", "scene-1", "s1"), "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 201);

    auto listRes = client->Get(("/api/projects/" + projectId + "/cues").c_str());
    REQUIRE(listRes != nullptr);
    REQUIRE(listRes->status == 200);
    auto cues = nlohmann::json::parse(listRes->body);
    REQUIRE(cues.is_array());
    REQUIRE(!cues.empty());

    auto updateBody = cueBody(projectId, "cue-1", "scene-1", "s1");
    auto updateJson = nlohmann::json::parse(updateBody);
    updateJson["name"] = "UpdatedCue";
    auto updateRes = client->Put(("/api/projects/" + projectId + "/cues/cue-1").c_str(), updateJson.dump(),
                                 "application/json");
    REQUIRE(updateRes != nullptr);
    REQUIRE(updateRes->status == 200);

    auto deleteRes = client->Delete(("/api/projects/" + projectId + "/cues/cue-1").c_str());
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 204);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API prevents deleting cues referenced by projects", "[http][integration][projects]") {
    auto dbPath = tempDbPath("http_api_project_cue_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-guard";
    createProject(*client, projectId);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                         "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/scenes").c_str(),
                         sceneWithSurfaceBody(projectId, "scene-1", "1"), "application/json")->status == 201);
    REQUIRE(client->Post(("/api/projects/" + projectId + "/cues").c_str(),
                         cueBody(projectId, "cue-guard", "scene-1", "s1"), "application/json")->status == 201);
    REQUIRE(client->Put(("/api/projects/" + projectId).c_str(),
                        projectBody(projectId, {"cue-guard"}), "application/json")->status == 200);

    auto deleteRes = client->Delete(("/api/projects/" + projectId + "/cues/cue-guard").c_str());
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 400);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API supports /api prefix", "[http][integration][api-prefix]") {
    auto dbPath = tempDbPath("http_api_prefix.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    createProject(*client, projectId);

    auto createRes = client->Post(("/api/projects/" + projectId + "/feeds").c_str(), feedBody(projectId),
                                  "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 201);

    auto listRes = client->Get(("/api/projects/" + projectId + "/feeds").c_str());
    REQUIRE(listRes != nullptr);
    REQUIRE(listRes->status == 200);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP server serves SPA static assets with fallback", "[http][integration][spa]") {
    auto dbPath = tempDbPath("http_api_spa.db");
    auto webRoot = tempWebRoot("lumi_spa_root");
    writeFile(webRoot / "index.html", "<html><body>SPA</body></html>");
    writeFile(webRoot / "assets/app.js", "console.log('ok');");

    TestServerContext ctx(dbPath, webRoot.string());
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto indexRes = client->Get("/");
    REQUIRE(indexRes != nullptr);
    REQUIRE(indexRes->status == 200);
    REQUIRE(indexRes->body.find("SPA") != std::string::npos);

    auto assetRes = client->Get("/assets/app.js");
    REQUIRE(assetRes != nullptr);
    REQUIRE(assetRes->status == 200);
    REQUIRE(assetRes->body.find("console.log") != std::string::npos);

    auto apiRes = client->Get("/api/projects/unknown");
    REQUIRE(apiRes != nullptr);
    REQUIRE(apiRes->status == 404);

    std::filesystem::remove(dbPath);
    std::filesystem::remove_all(webRoot);
}

}  // namespace projection::server
