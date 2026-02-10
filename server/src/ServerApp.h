#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "repo/CueRepository.h"

namespace projection::server::db {
class SqliteConnection;
}

namespace projection::server::repo {
class AssetRepository;
class FeedRepository;
class SceneRepository;
class CueRepository;
class ProjectRepository;
}

namespace projection::server::http {
class HttpServer;
}

namespace projection::server::renderer {
class RendererRegistry;
}

namespace projection::server {

struct ServerConfig {
    std::string databasePath;
    std::string webRoot;
    int httpPort;
    int rendererPort;
    bool verbose{false};
};

class ServerApp {
public:
    explicit ServerApp(ServerConfig config);
    ~ServerApp();

    int run();

    void stop();

private:
    ServerConfig config_;
    std::atomic<bool> stopRequested_{false};
    std::mutex stopMutex_;
    std::condition_variable stopCv_;
    std::unique_ptr<db::SqliteConnection> connection_;
    std::unique_ptr<repo::AssetRepository> assetRepository_;
    std::unique_ptr<repo::FeedRepository> feedRepository_;
    std::unique_ptr<repo::SceneRepository> sceneRepository_;
    std::unique_ptr<repo::CueRepository> cueRepository_;
    std::unique_ptr<repo::ProjectRepository> projectRepository_;
    std::unique_ptr<http::HttpServer> httpServer_;
    std::shared_ptr<renderer::RendererRegistry> rendererRegistry_;
};

}  // namespace projection::server
