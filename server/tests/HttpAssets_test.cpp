#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/AssetRepository.h"
#include "repo/CueRepository.h"
#include "repo/FeedRepository.h"
#include "repo/ProjectRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::out | std::ios::binary);
    REQUIRE(file);
    file << contents;
}

class ServerRunner {
public:
    ServerRunner(http::HttpServer& server, int port) : server_(server) {
        thread_ = std::thread([this, port] { server_.start(port); });
    }

    ~ServerRunner() {
        if (thread_.joinable()) {
            server_.stop();
            thread_.join();
        }
    }

private:
    http::HttpServer& server_;
    std::thread thread_;
};

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

bool waitForServer(httplib::Client& client, http::HttpServer& server) {
    for (int attempt = 0; attempt < 100; ++attempt) {
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

}  // namespace

TEST_CASE("GET /api/assets lists asset records", "[http][assets]") {
    const auto base = std::filesystem::temp_directory_path() / "lumi-assets-test";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base / "data" / "assets");

    const auto previousPath = std::filesystem::current_path();
    std::filesystem::current_path(base);

    db::SqliteConnection connection;
    connection.open(tempDbPath("lumi-assets.db"));
    db::SchemaMigrations::applyMigrations(connection);

    repo::AssetRepository assetRepo(connection);
    repo::FeedRepository feedRepo(connection);
    repo::SceneRepository sceneRepo(connection);
    repo::CueRepository cueRepo(connection);
    repo::ProjectRepository projectRepo(connection);

    assetRepo.createAsset(core::Asset{core::AssetId{}, "Clip A", core::AssetType::VideoFile, "clipA.mp4"});
    assetRepo.createAsset(core::Asset{core::AssetId{}, "Poster", core::AssetType::ImageFile, "poster.png"});

    http::HttpServer httpServer(assetRepo, feedRepo, sceneRepo, cueRepo, projectRepo, nullptr, true, "");

    const int port = reservePort();
    ServerRunner runner(httpServer, port);
    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, httpServer));

    auto res = client->Get("/api/assets");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->body.find("Clip A") != std::string::npos);
    REQUIRE(res->body.find("Poster") != std::string::npos);

    std::filesystem::current_path(previousPath);
    std::filesystem::remove_all(base);
}

TEST_CASE("POST /api/assets uploads a new asset", "[http][assets]") {
    const auto base = std::filesystem::temp_directory_path() / "lumi-assets-upload-test";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base / "data" / "assets");

    const auto previousPath = std::filesystem::current_path();
    std::filesystem::current_path(base);

    db::SqliteConnection connection;
    connection.open(tempDbPath("lumi-assets-upload.db"));
    db::SchemaMigrations::applyMigrations(connection);

    repo::AssetRepository assetRepo(connection);
    repo::FeedRepository feedRepo(connection);
    repo::SceneRepository sceneRepo(connection);
    repo::CueRepository cueRepo(connection);
    repo::ProjectRepository projectRepo(connection);
    http::HttpServer httpServer(assetRepo, feedRepo, sceneRepo, cueRepo, projectRepo, nullptr, true, "");

    const int port = reservePort();
    ServerRunner runner(httpServer, port);
    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, httpServer));

    httplib::MultipartFormDataItems items = {
        {"file", "dummy", "clipC.mp4", "application/octet-stream"},
    };
    auto res = client->Post("/api/assets", items);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 201);
    REQUIRE(std::filesystem::exists(base / "data" / "assets" / "clipC.mp4"));

    std::filesystem::current_path(previousPath);
    std::filesystem::remove_all(base);
}

TEST_CASE("DELETE /api/assets/{id} removes an asset", "[http][assets]") {
    const auto base = std::filesystem::temp_directory_path() / "lumi-assets-delete-test";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base / "data" / "assets");

    const auto targetPath = base / "data" / "assets" / "clipD.mp4";
    writeFile(targetPath, "dummy");

    const auto previousPath = std::filesystem::current_path();
    std::filesystem::current_path(base);

    db::SqliteConnection connection;
    connection.open(tempDbPath("lumi-assets-delete.db"));
    db::SchemaMigrations::applyMigrations(connection);

    repo::AssetRepository assetRepo(connection);
    repo::FeedRepository feedRepo(connection);
    repo::SceneRepository sceneRepo(connection);
    repo::CueRepository cueRepo(connection);
    repo::ProjectRepository projectRepo(connection);
    auto asset = assetRepo.createAsset(core::Asset{core::AssetId{}, "Clip D", core::AssetType::VideoFile,
                                                   targetPath.string()});

    http::HttpServer httpServer(assetRepo, feedRepo, sceneRepo, cueRepo, projectRepo, nullptr, true, "");

    const int port = reservePort();
    ServerRunner runner(httpServer, port);
    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, httpServer));

    auto res = client->Delete(std::string("/api/assets/") + asset.getId().value);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 204);
    REQUIRE(!std::filesystem::exists(targetPath));

    std::filesystem::current_path(previousPath);
    std::filesystem::remove_all(base);
}

}  // namespace projection::server

