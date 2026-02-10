#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "projection/core/Asset.h"
#include "repo/AssetRepository.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <httplib.h>
#include <memory>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::http::HttpServer;
using projection::server::repo::AssetRepository;
using projection::server::repo::FeedRepository;
using projection::server::repo::SceneRepository;
using projection::server::repo::CueRepository;
using projection::server::repo::ProjectRepository;
namespace core = projection::core;

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

std::string tempDb(const std::string& name) { return (std::filesystem::temp_directory_path() / name).string(); }

struct TestServerContext {
    SqliteConnection connection;
    AssetRepository assetRepo;
    FeedRepository feedRepo;
    SceneRepository sceneRepo;
    CueRepository cueRepo;
    ProjectRepository projectRepo;
    HttpServer httpServer;

    explicit TestServerContext(const std::string& dbPath)
        : assetRepo(connection),
          feedRepo(connection),
          sceneRepo(connection),
          cueRepo(connection),
          projectRepo(connection),
          httpServer(assetRepo, feedRepo, sceneRepo, cueRepo, projectRepo, nullptr) {
        connection.open(dbPath);
        SchemaMigrations::applyMigrations(connection);
    }
};

class ServerRunner {
public:
    ServerRunner(HttpServer& server, int port) : server_(server) {
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
    HttpServer& server_;
    std::thread thread_;
    std::exception_ptr startError_;
};

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

bool waitForServer(httplib::Client& client, HttpServer& server, const ServerRunner& runner) {
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

nlohmann::json surfaceJson(const std::string& id, const std::string& feedId, int zOrder) {
    nlohmann::json vertices = nlohmann::json::array({{{"x", 0.0}, {"y", 0.0}}, {{"x", 1.0}, {"y", 0.0}},
                                                    {{"x", 1.0}, {"y", 1.0}}, {{"x", 0.0}, {"y", 1.0}}});
    return nlohmann::json{{"id", id},
                          {"name", id + "name"},
                          {"vertices", vertices},
                          {"feedId", feedId},
                          {"opacity", 1.0},
                          {"brightness", 1.0},
                          {"blendMode", "Normal"},
                          {"zOrder", zOrder}};
}

nlohmann::json feedJson(const std::string& projectId, const std::string& id, const std::string& assetId) {
    return nlohmann::json{{"projectId", projectId},
                          {"id", id},
                          {"name", "Feed" + id},
                          {"assetId", assetId},
                          {"settings", nlohmann::json::object()}};
}

}  // namespace

TEST_CASE("HTTP scenes endpoint persists and returns surfaces", "[http][scenes]") {
    auto dbPath = tempDb("http_scenes_surfaces.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    REQUIRE(client->Post("/api/projects", nlohmann::json{{"id", projectId},
                                                         {"name", "Project"},
                                                         {"description", ""},
                                                         {"createdAt", "2026-02-02T10:00:00Z"},
                                                         {"updatedAt", "2026-02-02T10:00:00Z"},
                                                         {"assetIds", nlohmann::json::array()},
                                                         {"sceneIds", nlohmann::json::array()},
                                                         {"feedIds", nlohmann::json::array()},
                                                         {"cueOrder", nlohmann::json::array()},
                                                         {"settings", nlohmann::json::object()}}
                                                         .dump(),
                         "application/json")
                ->status == 201);

    auto assetA =
        ctx.assetRepo.createAsset(core::Asset{core::AssetId{}, "A", core::AssetType::VideoFile, "/videos/a.mp4"});
    auto assetB =
        ctx.assetRepo.createAsset(core::Asset{core::AssetId{}, "B", core::AssetType::VideoFile, "/videos/b.mp4"});
    ctx.assetRepo.addAssetToProject(core::ProjectId{projectId}, assetA.getId());
    ctx.assetRepo.addAssetToProject(core::ProjectId{projectId}, assetB.getId());
    auto project = ctx.projectRepo.findProjectById(core::ProjectId{projectId});
    REQUIRE(project.has_value());
    project->getAssetIds() = {assetA.getId(), assetB.getId()};
    project->setUpdatedAt("2026-02-02T10:00:00Z");
    ctx.projectRepo.updateProject(*project);

    auto feed1 = client->Post(("/api/projects/" + projectId + "/feeds").c_str(),
                              feedJson(projectId, "1", assetA.getId().value).dump(), "application/json");
    REQUIRE(feed1 != nullptr);
    REQUIRE(feed1->status == 201);
    auto feed2 = client->Post(("/api/projects/" + projectId + "/feeds").c_str(),
                              feedJson(projectId, "2", assetB.getId().value).dump(), "application/json");
    REQUIRE(feed2 != nullptr);
    REQUIRE(feed2->status == 201);

    nlohmann::json scene{{"projectId", projectId},
                         {"id", "10"},
                         {"name", "Scene1"},
                         {"description", "desc"},
                         {"surfaces", nlohmann::json::array({surfaceJson("s1", "1", 0), surfaceJson("s2", "2", 1)})}};

    auto postScene = client->Post(("/api/projects/" + projectId + "/scenes").c_str(), scene.dump(), "application/json");
    REQUIRE(postScene != nullptr);
    REQUIRE(postScene->status == 201);

    auto getScene = client->Get(("/api/projects/" + projectId + "/scenes/10").c_str());
    REQUIRE(getScene != nullptr);
    REQUIRE(getScene->status == 200);

    auto bodyJson = nlohmann::json::parse(getScene->body);
    REQUIRE(bodyJson["surfaces"].is_array());
    REQUIRE(bodyJson["surfaces"].size() == 2);
    REQUIRE(bodyJson["surfaces"][0]["feedId"] == "1");
    REQUIRE(bodyJson["surfaces"][1]["feedId"] == "2");

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP scenes endpoint validates feed references", "[http][scenes][validation]") {
    auto dbPath = tempDb("http_scenes_surfaces_invalid.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    const std::string projectId = "project-1";
    REQUIRE(client->Post("/api/projects", nlohmann::json{{"id", projectId},
                                                         {"name", "Project"},
                                                         {"description", ""},
                                                         {"createdAt", "2026-02-02T10:00:00Z"},
                                                         {"updatedAt", "2026-02-02T10:00:00Z"},
                                                         {"assetIds", nlohmann::json::array()},
                                                         {"sceneIds", nlohmann::json::array()},
                                                         {"feedIds", nlohmann::json::array()},
                                                         {"cueOrder", nlohmann::json::array()},
                                                         {"settings", nlohmann::json::object()}}
                                                         .dump(),
                         "application/json")
                ->status == 201);

    auto asset =
        ctx.assetRepo.createAsset(core::Asset{core::AssetId{}, "A", core::AssetType::VideoFile, "/videos/a.mp4"});
    ctx.assetRepo.addAssetToProject(core::ProjectId{projectId}, asset.getId());
    auto project = ctx.projectRepo.findProjectById(core::ProjectId{projectId});
    REQUIRE(project.has_value());
    project->getAssetIds() = {asset.getId()};
    project->setUpdatedAt("2026-02-02T10:00:00Z");
    ctx.projectRepo.updateProject(*project);

    auto feed1 = client->Post(("/api/projects/" + projectId + "/feeds").c_str(),
                              feedJson(projectId, "1", asset.getId().value).dump(), "application/json");
    REQUIRE(feed1 != nullptr);
    REQUIRE(feed1->status == 201);

    nlohmann::json scene{{"projectId", projectId},
                         {"id", "11"},
                         {"name", "Scene2"},
                         {"description", "desc"},
                         {"surfaces", nlohmann::json::array({surfaceJson("s1", "missing", 0)})}};

    auto postScene = client->Post(("/api/projects/" + projectId + "/scenes").c_str(), scene.dump(), "application/json");
    REQUIRE(postScene != nullptr);
    REQUIRE(postScene->status == 400);

    std::filesystem::remove(dbPath);
}
