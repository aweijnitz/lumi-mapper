#include "http/HttpServer.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "projection/core/Serialization.h"
#include "projection/core/RendererProtocol.h"
#include "projection/core/Validation.h"
#include "projection/core/Project.h"
#include "projection/core/Asset.h"

namespace projection::server::http {

using nlohmann::json;

namespace {
void applyCueToScene(const core::Cue& cue, core::Scene& scene) {
    for (auto& surface : scene.getSurfaces()) {
        auto opacityIt = cue.getSurfaceOpacities().find(surface.getId());
        if (opacityIt != cue.getSurfaceOpacities().end()) {
            surface.setOpacity(opacityIt->second);
        }
        auto brightnessIt = cue.getSurfaceBrightnesses().find(surface.getId());
        if (brightnessIt != cue.getSurfaceBrightnesses().end()) {
            surface.setBrightness(brightnessIt->second);
        }
    }
}

std::optional<std::filesystem::path> findAssetsRoot() {
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "data" / "assets",
        std::filesystem::current_path().parent_path() / "data" / "assets",
        std::filesystem::current_path().parent_path().parent_path() / "data" / "assets"};
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }
    return std::nullopt;
}

std::optional<core::AssetType> assetTypeForPath(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    // Convert to lowercase for case-insensitive comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".mkv" || ext == ".webm" || ext == ".m4v") {
        return core::AssetType::VideoFile;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp" || ext == ".bmp" ||
        ext == ".tiff" || ext == ".tif") {
        return core::AssetType::ImageFile;
    }
    return std::nullopt;
}

std::optional<std::string> sanitizeAssetName(const std::string& raw) {
    std::filesystem::path candidate(raw);
    const auto name = candidate.filename().string();
    if (name.empty() || name == "." || name == "..") {
        return std::nullopt;
    }
    return name;
}

std::string nowIso8601Utc() {
    using std::chrono::system_clock;
    auto now = system_clock::now();
    std::time_t nowTime = system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &nowTime);
#else
    gmtime_r(&nowTime, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
}  // namespace

HttpServer::HttpServer(repo::AssetRepository& assetRepository, repo::FeedRepository& feedRepository,
                       repo::SceneRepository& sceneRepository, repo::CueRepository& cueRepository,
                       repo::ProjectRepository& projectRepository,
                       std::shared_ptr<renderer::RendererRegistry> rendererRegistry, bool verbose,
                       std::string webRoot)
    : feedRepository_(feedRepository),
      assetRepository_(assetRepository),
      sceneRepository_(sceneRepository),
      cueRepository_(cueRepository),
      projectRepository_(projectRepository),
      rendererRegistry_(std::move(rendererRegistry)),
      server_(std::make_unique<::httplib::Server>()),
      verbose_(verbose),
      webRoot_(std::move(webRoot)) {
    registerRoutes();
}

void HttpServer::start(int port) {
    if (!server_->listen("0.0.0.0", port)) {
        throw std::runtime_error("Failed to start HTTP server on port " + std::to_string(port));
    }
}

void HttpServer::stop() { server_->stop(); }

bool HttpServer::isRunning() const { return server_ && server_->is_running(); }

void HttpServer::registerRoutes() {
    auto log = [&](const std::string& msg) {
        if (verbose_) {
            std::cerr << "[http] " << msg << std::endl;
        }
    };
    (void)log;

    auto registerGet = [this](const std::string& path, ::httplib::Server::Handler handler) {
        server_->Get(path, handler);
        server_->Get("/api" + path, handler);
    };
    auto registerPost = [this](const std::string& path, ::httplib::Server::Handler handler) {
        server_->Post(path, handler);
        server_->Post("/api" + path, handler);
    };
    auto registerPut = [this](const std::string& path, ::httplib::Server::Handler handler) {
        server_->Put(path, handler);
        server_->Put("/api" + path, handler);
    };
    auto registerDelete = [this](const std::string& path, ::httplib::Server::Handler handler) {
        server_->Delete(path, handler);
        server_->Delete("/api" + path, handler);
    };

    auto updateProjectAssetList = [this](const core::ProjectId& projectId, const core::AssetId& assetId,
                                         bool add, ::httplib::Response& res) -> bool {
        auto projectOpt = projectRepository_.findProjectById(projectId);
        if (!projectOpt.has_value()) {
            respondWithError(res, 404, "Project not found");
            return false;
        }
        auto project = *projectOpt;
        auto& assetIds = project.getAssetIds();
        if (add) {
            auto it = std::find_if(assetIds.begin(), assetIds.end(),
                                   [&](const core::AssetId& id) { return id == assetId; });
            if (it == assetIds.end()) {
                assetIds.push_back(assetId);
            }
        } else {
            assetIds.erase(std::remove_if(assetIds.begin(), assetIds.end(),
                                          [&](const core::AssetId& id) { return id == assetId; }),
                           assetIds.end());
        }
        project.setUpdatedAt(nowIso8601Utc());
        projectRepository_.updateProject(project);
        return true;
    };
    auto updateProjectFeedList = [this](const core::ProjectId& projectId, const core::FeedId& feedId,
                                        bool add, ::httplib::Response& res) -> bool {
        auto projectOpt = projectRepository_.findProjectById(projectId);
        if (!projectOpt.has_value()) {
            respondWithError(res, 404, "Project not found");
            return false;
        }
        auto project = *projectOpt;
        auto& feedIds = project.getFeedIds();
        if (add) {
            auto it = std::find_if(feedIds.begin(), feedIds.end(),
                                   [&](const core::FeedId& id) { return id == feedId; });
            if (it == feedIds.end()) {
                feedIds.push_back(feedId);
            }
        } else {
            feedIds.erase(std::remove_if(feedIds.begin(), feedIds.end(),
                                         [&](const core::FeedId& id) { return id == feedId; }),
                          feedIds.end());
        }
        project.setUpdatedAt(nowIso8601Utc());
        projectRepository_.updateProject(project);
        return true;
    };
    auto updateProjectSceneList = [this](const core::ProjectId& projectId, const core::SceneId& sceneId,
                                         bool add, ::httplib::Response& res) -> bool {
        auto projectOpt = projectRepository_.findProjectById(projectId);
        if (!projectOpt.has_value()) {
            respondWithError(res, 404, "Project not found");
            return false;
        }
        auto project = *projectOpt;
        auto& sceneIds = project.getSceneIds();
        if (add) {
            auto it = std::find_if(sceneIds.begin(), sceneIds.end(),
                                   [&](const core::SceneId& id) { return id == sceneId; });
            if (it == sceneIds.end()) {
                sceneIds.push_back(sceneId);
            }
        } else {
            sceneIds.erase(std::remove_if(sceneIds.begin(), sceneIds.end(),
                                          [&](const core::SceneId& id) { return id == sceneId; }),
                           sceneIds.end());
        }
        project.setUpdatedAt(nowIso8601Utc());
        projectRepository_.updateProject(project);
        return true;
    };
    auto updateProjectCueOrder = [this](const core::ProjectId& projectId, const core::CueId& cueId,
                                        bool add, ::httplib::Response& res) -> bool {
        auto projectOpt = projectRepository_.findProjectById(projectId);
        if (!projectOpt.has_value()) {
            respondWithError(res, 404, "Project not found");
            return false;
        }
        auto project = *projectOpt;
        auto& cueOrder = project.getCueOrder();
        if (add) {
            auto it = std::find_if(cueOrder.begin(), cueOrder.end(),
                                   [&](const core::CueId& id) { return id == cueId; });
            if (it == cueOrder.end()) {
                cueOrder.push_back(cueId);
            }
        } else {
            cueOrder.erase(std::remove_if(cueOrder.begin(), cueOrder.end(),
                                          [&](const core::CueId& id) { return id == cueId; }),
                           cueOrder.end());
        }
        project.setUpdatedAt(nowIso8601Utc());
        projectRepository_.updateProject(project);
        return true;
    };

    registerPost(R"(/projects/([^/]+)/feeds)",
                 [this, updateProjectAssetList, updateProjectFeedList](const ::httplib::Request& req,
                                                                       ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto feed = body.get<core::Feed>();
            if (feed.getProjectId() != projectId) {
                respondWithError(res, 400, "Feed projectId does not match path project id");
                return;
            }
            auto asset = assetRepository_.findAssetById(feed.getAssetId());
            if (!asset.has_value()) {
                respondWithError(res, 400, "Feed asset does not exist");
                return;
            }
            auto projectOpt = projectRepository_.findProjectById(projectId);
            if (!projectOpt.has_value()) {
                respondWithError(res, 404, "Project not found");
                return;
            }
            const auto& assetIds = projectOpt->getAssetIds();
            auto assetIt = std::find_if(assetIds.begin(), assetIds.end(),
                                        [&](const core::AssetId& id) { return id == feed.getAssetId(); });
            if (assetIt == assetIds.end()) {
                assetRepository_.addAssetToProject(projectId, feed.getAssetId());
                if (!updateProjectAssetList(projectId, feed.getAssetId(), true, res)) {
                    return;
                }
            }
            auto created = feedRepository_.createFeed(feed);
            updateProjectFeedList(projectId, created.getId(), true, res);
            if (verbose_) {
                std::cerr << "[http] Created feed project=" << created.getProjectId().value
                          << " id=" << created.getId().value << " name=" << created.getName()
                          << std::endl;
            }
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerPut(R"(/projects/([^/]+)/feeds/(.+))",
                [this, updateProjectAssetList](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or feed id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto feed = body.get<core::Feed>();
            if (feed.getProjectId() != projectId) {
                respondWithError(res, 400, "Feed projectId does not match path project id");
                return;
            }
            auto asset = assetRepository_.findAssetById(feed.getAssetId());
            if (!asset.has_value()) {
                respondWithError(res, 400, "Feed asset does not exist");
                return;
            }
            auto projectOpt = projectRepository_.findProjectById(projectId);
            if (!projectOpt.has_value()) {
                respondWithError(res, 404, "Project not found");
                return;
            }
            const auto& assetIds = projectOpt->getAssetIds();
            auto assetIt = std::find_if(assetIds.begin(), assetIds.end(),
                                        [&](const core::AssetId& id) { return id == feed.getAssetId(); });
            if (assetIt == assetIds.end()) {
                assetRepository_.addAssetToProject(projectId, feed.getAssetId());
                if (!updateProjectAssetList(projectId, feed.getAssetId(), true, res)) {
                    return;
                }
            }
            feed.setId(core::FeedId{req.matches[2]});
            auto updated = feedRepository_.updateFeed(feed);
            res.status = 200;
            res.set_content(json(updated).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/feeds/(.+))",
                   [this, updateProjectFeedList](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or feed id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            core::FeedId feedId{req.matches[2]};
            // Guard: ensure no surfaces reference this feed
            auto scenes = sceneRepository_.listScenes(projectId);
            for (const auto& scene : scenes) {
                for (const auto& surface : scene.getSurfaces()) {
                    if (surface.getFeedId() == feedId) {
                        respondWithError(res, 400,
                                         "Cannot delete feed " + feedId.value + " because it is referenced by scene " +
                                             scene.getId().value + ".");
                        return;
                    }
                }
            }
            feedRepository_.deleteFeed(projectId, feedId);
            updateProjectFeedList(projectId, feedId, false, res);
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerGet(R"(/projects/([^/]+)/feeds)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            const auto feeds = feedRepository_.listFeeds(projectId);
            res.status = 200;
            res.set_content(json(feeds).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost(R"(/projects/([^/]+)/scenes)",
                 [this, updateProjectSceneList](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto scene = body.get<core::Scene>();
            if (scene.getProjectId() != projectId) {
                respondWithError(res, 400, "Scene projectId does not match path project id");
                return;
            }
            if (verbose_) {
                std::cerr << "[http] Received scene create project=" << scene.getProjectId().value
                          << " id=" << scene.getId().value << " name=" << scene.getName()
                          << " surfaces=" << scene.getSurfaces().size() << std::endl;
            }

            auto feeds = feedRepository_.listFeeds(projectId);
            std::string error;
            if (!core::validateSceneFeeds(scene, feeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

            auto created = sceneRepository_.createScene(scene);
            updateProjectSceneList(projectId, created.getId(), true, res);
            if (verbose_) {
                std::cerr << "[http] Created scene id=" << created.getId().value << std::endl;
            }
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerPut(R"(/projects/([^/]+)/scenes/(.+))",
                [this, updateProjectSceneList](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or scene id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto scene = body.get<core::Scene>();
            if (scene.getProjectId() != projectId) {
                respondWithError(res, 400, "Scene projectId does not match path project id");
                return;
            }
            scene.setId(core::SceneId{req.matches[2]});

            auto feeds = feedRepository_.listFeeds(projectId);
            std::string error;
            if (!core::validateSceneFeeds(scene, feeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

            auto updated = sceneRepository_.updateScene(scene);
            updateProjectSceneList(projectId, updated.getId(), true, res);
            res.status = 200;
            res.set_content(json(updated).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/scenes/(.+))",
                   [this, updateProjectSceneList](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or scene id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            core::SceneId sceneId{req.matches[2]};
            // Guard: ensure no cues reference this scene
            auto cues = cueRepository_.listCues(projectId);
            for (const auto& cue : cues) {
                if (cue.getSceneId() == sceneId) {
                    respondWithError(res, 400,
                                     "Cannot delete scene " + sceneId.value + " because it is referenced by cue " +
                                         cue.getId().value + ".");
                    return;
                }
            }
            sceneRepository_.deleteScene(projectId, sceneId);
            updateProjectSceneList(projectId, sceneId, false, res);
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerGet(R"(/projects/([^/]+)/cues)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            const auto cues = cueRepository_.listCues(projectId);
            res.status = 200;
            res.set_content(json(cues).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost(R"(/projects/([^/]+)/cues)",
                 [this, updateProjectCueOrder](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto cue = body.get<core::Cue>();
            if (cue.getProjectId() != projectId) {
                respondWithError(res, 400, "Cue projectId does not match path project id");
                return;
            }
            auto scene = sceneRepository_.findSceneById(projectId, cue.getSceneId());
            if (!scene.has_value()) {
                respondWithError(res, 400, "Scene does not exist for cue");
                return;
            }
            std::string error;
            if (!core::validateCueForScene(cue, *scene, error)) {
                respondWithError(res, 400, error);
                return;
            }
            auto created = cueRepository_.createCue(cue);
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerPut(R"(/projects/([^/]+)/cues/(.+))",
                [this, updateProjectCueOrder](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or cue id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            auto cue = body.get<core::Cue>();
            if (cue.getProjectId() != projectId) {
                respondWithError(res, 400, "Cue projectId does not match path project id");
                return;
            }
            cue.setId(core::CueId{req.matches[2]});
            auto scene = sceneRepository_.findSceneById(projectId, cue.getSceneId());
            if (!scene.has_value()) {
                respondWithError(res, 400, "Scene does not exist for cue");
                return;
            }
            std::string error;
            if (!core::validateCueForScene(cue, *scene, error)) {
                respondWithError(res, 400, error);
                return;
            }
            cueRepository_.updateCue(cue);
            res.status = 200;
            res.set_content(json(cue).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/cues/(.+))",
                   [this, updateProjectCueOrder](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or cue id");
                return;
            }
            const auto projectId = core::ProjectId{req.matches[1]};
            const auto cueId = core::CueId{req.matches[2]};
            // Guard: ensure no projects reference this cue
            auto project = projectRepository_.findProjectById(projectId);
            if (project.has_value()) {
                for (const auto& projectCueId : project->getCueOrder()) {
                    if (projectCueId == cueId) {
                        respondWithError(res, 400,
                                         "Cannot delete cue " + cueId.value + " because it is referenced by project " +
                                             projectId.value + ".");
                        return;
                    }
                }
            }
            cueRepository_.deleteCue(projectId, cueId);
            updateProjectCueOrder(projectId, cueId, false, res);
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerGet(R"(/projects/([^/]+)/scenes)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            const auto scenes = sceneRepository_.listScenes(projectId);
            res.status = 200;
            res.set_content(json(scenes).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerGet(R"(/projects/([^/]+)/scenes/(.+))",
                [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or scene id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            core::SceneId sceneId{req.matches[2]};
            auto scene = sceneRepository_.findSceneById(projectId, sceneId);
            if (!scene.has_value()) {
                respondWithError(res, 404, "Scene not found");
                return;
            }

            res.status = 200;
            res.set_content(json(*scene).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerGet("/projects", [this, log](const ::httplib::Request&, ::httplib::Response& res) {
        try {
            const auto projects = projectRepository_.listProjects();
            log("Listing projects count=" + std::to_string(projects.size()));
            res.status = 200;
            res.set_content(json(projects).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerGet(R"(/projects/(.+))", [this, log](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            auto projectId = core::ProjectId{req.matches[1]};
            log("Fetching project id=" + projectId.value);
            auto project = projectRepository_.findProjectById(projectId);
            if (!project.has_value()) {
                respondWithError(res, 404, "Project not found");
                return;
            }
            res.status = 200;
            res.set_content(json(*project).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost("/projects", [this, log](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto project = body.get<core::Project>();
            log("Received project create id=" + project.getId().value + " name=" + project.getName() +
                " cueCount=" + std::to_string(project.getCueOrder().size()));
            auto assets = assetRepository_.listAssets();
            auto feeds = feedRepository_.listFeeds(project.getId());
            auto scenes = sceneRepository_.listScenes(project.getId());
            auto cues = cueRepository_.listCues(project.getId());
            std::string error;
            if (!core::validateProjectCues(project, assets, feeds, scenes, cues, error)) {
                respondWithError(res, 400, error);
                return;
            }
            auto created = projectRepository_.createProject(project);
            log("Created project id=" + created.getId().value);
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerPut(R"(/projects/(.+))", [this, log](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            auto body = json::parse(req.body);
            auto project = body.get<core::Project>();
            project.setId(core::ProjectId{req.matches[1]});
            log("Received project update id=" + project.getId().value + " name=" + project.getName() +
                " cueCount=" + std::to_string(project.getCueOrder().size()));
            auto assets = assetRepository_.listAssets();
            auto feeds = feedRepository_.listFeeds(project.getId());
            auto scenes = sceneRepository_.listScenes(project.getId());
            auto cues = cueRepository_.listCues(project.getId());
            std::string error;
            if (!core::validateProjectCues(project, assets, feeds, scenes, cues, error)) {
                respondWithError(res, 400, error);
                return;
            }
            auto updated = projectRepository_.updateProject(project);
            log("Updated project id=" + updated.getId().value);
            res.status = 200;
            res.set_content(json(updated).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/(.+))", [this, log](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            log("Deleting project id=" + std::string(req.matches[1]));
            projectRepository_.deleteProject(core::ProjectId{req.matches[1]});
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerGet("/assets", [this](const ::httplib::Request&, ::httplib::Response& res) {
        try {
            const auto assets = assetRepository_.listAssets();
            res.status = 200;
            res.set_content(json(assets).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerGet(R"(/projects/([^/]+)/assets)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            const auto assets = assetRepository_.listAssetsForProject(projectId);
            res.status = 200;
            res.set_content(json(assets).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    auto handleAssetUpload = [this, updateProjectAssetList](const ::httplib::Request& req,
                                                            ::httplib::Response& res,
                                                            const std::optional<core::ProjectId>& projectId) {
        constexpr uint64_t kMaxAssetBytes = 2ull * 1024ull * 1024ull * 1024ull;
        try {
            auto lengthHeader = req.get_header_value("Content-Length");
            if (!lengthHeader.empty()) {
                try {
                    const auto length = static_cast<uint64_t>(std::stoull(lengthHeader));
                    if (length > kMaxAssetBytes) {
                        respondWithError(res, 413, "Asset upload exceeds 2GB limit.");
                        return;
                    }
                } catch (const std::exception&) {
                    respondWithError(res, 400, "Invalid Content-Length header.");
                    return;
                }
            }

            if (!req.is_multipart_form_data()) {
                respondWithError(res, 400, "Expected multipart/form-data upload.");
                return;
            }

            auto file = req.get_file_value("file");
            if (file.filename.empty()) {
                respondWithError(res, 400, "Missing file upload.");
                return;
            }

            const auto safeName = sanitizeAssetName(file.filename);
            if (!safeName.has_value()) {
                respondWithError(res, 400, "Invalid asset filename.");
                return;
            }

            auto root = findAssetsRoot();
            if (!root.has_value()) {
                const auto fallbackRoot = std::filesystem::current_path() / "data" / "assets";
                std::filesystem::create_directories(fallbackRoot);
                root = std::filesystem::weakly_canonical(fallbackRoot);
            }

            const auto targetPath = *root / *safeName;
            if (std::filesystem::exists(targetPath)) {
                respondWithError(res, 409, "Asset already exists.");
                return;
            }

            if (static_cast<uint64_t>(file.content.size()) > kMaxAssetBytes) {
                respondWithError(res, 413, "Asset upload exceeds 2GB limit.");
                return;
            }

            std::ofstream output(targetPath, std::ios::binary | std::ios::out);
            if (!output) {
                respondWithError(res, 500, "Failed to open asset destination.");
                return;
            }
            output.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
            if (!output) {
                respondWithError(res, 500, "Failed to write asset.");
                return;
            }

            const auto assetType = assetTypeForPath(targetPath);
            if (!assetType.has_value()) {
                respondWithError(res, 400, "Unsupported asset type.");
                return;
            }

            core::Asset asset{core::AssetId{}, *safeName, *assetType, targetPath.string(), {}};
            auto created = assetRepository_.createAsset(asset);

            if (projectId.has_value()) {
                assetRepository_.addAssetToProject(*projectId, created.getId());
                updateProjectAssetList(*projectId, created.getId(), true, res);
            }

            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    };

    registerPost("/assets", [this, handleAssetUpload](const ::httplib::Request& req, ::httplib::Response& res) {
        handleAssetUpload(req, res, std::nullopt);
    });

    registerPost(R"(/projects/([^/]+)/assets)", [this, handleAssetUpload](const ::httplib::Request& req,
                                                                         ::httplib::Response& res) {
        if (req.matches.size() < 2) {
            respondWithError(res, 400, "Missing project id");
            return;
        }
        core::ProjectId projectId{req.matches[1]};
        handleAssetUpload(req, res, projectId);
    });

    registerPost(R"(/projects/([^/]+)/assets/(.+))", [this, updateProjectAssetList](const ::httplib::Request& req,
                                                                                  ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or asset id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            core::AssetId assetId{req.matches[2]};
            auto asset = assetRepository_.findAssetById(assetId);
            if (!asset.has_value()) {
                respondWithError(res, 404, "Asset not found");
                return;
            }
            assetRepository_.addAssetToProject(projectId, assetId);
            if (!updateProjectAssetList(projectId, assetId, true, res)) {
                return;
            }
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/assets/(.+))", [this, updateProjectAssetList](const ::httplib::Request& req,
                                                                                    ::httplib::Response& res) {
        try {
            if (req.matches.size() < 3) {
                respondWithError(res, 400, "Missing project or asset id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            core::AssetId assetId{req.matches[2]};
            assetRepository_.removeAssetFromProject(projectId, assetId);
            if (!updateProjectAssetList(projectId, assetId, false, res)) {
                return;
            }
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerDelete(R"(/assets/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing asset id");
                return;
            }
            core::AssetId assetId{req.matches[1]};
            auto asset = assetRepository_.findAssetById(assetId);
            if (!asset.has_value()) {
                respondWithError(res, 404, "Asset not found.");
                return;
            }

            const auto targetPath = std::filesystem::path(asset->getPath());
            if (std::filesystem::exists(targetPath)) {
                std::filesystem::remove(targetPath);
            }
            assetRepository_.deleteAsset(assetId);
            // Ensure projects drop the asset id from their lists.
            auto projects = projectRepository_.listProjects();
            for (auto& project : projects) {
                auto& assetIds = project.getAssetIds();
                auto before = assetIds.size();
                assetIds.erase(std::remove_if(assetIds.begin(), assetIds.end(),
                                              [&](const core::AssetId& id) { return id == assetId; }),
                               assetIds.end());
                if (assetIds.size() != before) {
                    project.setUpdatedAt(nowIso8601Utc());
                    projectRepository_.updateProject(project);
                }
            }
            res.status = 204;
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    auto handleRendererPing = [this](const ::httplib::Request&, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        const auto renderers = rendererRegistry_->rendererInfo();
        if (renderers.empty()) {
            respondWithError(res, 503, "No renderers connected");
            return;
        }

        json rendererPayload = json::array();
        for (const auto& renderer : renderers) {
            rendererPayload.push_back(
                json{{"name", renderer.name}, {"width", renderer.width}, {"height", renderer.height}});
        }

        res.status = 200;
        res.set_content(json({{"status", "ok"}, {"renderers", rendererPayload}}).dump(), "application/json");
    };

    registerPost("/renderer/ping", handleRendererPing);
    registerGet("/renderer/ping", handleRendererPing);

    registerPost(R"(/projects/([^/]+)/renderer/loadScene)",
                 [this](const ::httplib::Request& req, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            if (!body.contains("sceneId") || !body["sceneId"].is_string()) {
                respondWithError(res, 400, "Missing or invalid sceneId");
                return;
            }

            core::SceneId sceneId{body["sceneId"].get<std::string>()};
            auto scene = sceneRepository_.findSceneById(projectId, sceneId);
            if (!scene.has_value()) {
                respondWithError(res, 400, "Scene does not exist");
                return;
            }

            std::vector<core::Feed> feeds;
            std::string error;
            if (!collectFeedsForScene(*scene, feeds, error)) {
                respondWithError(res, 400, error);
                return;
            }
            std::vector<core::Asset> assets;
            if (!collectAssetsForFeeds(feeds, assets, error)) {
                respondWithError(res, 400, error);
                return;
            }

            if (verbose_) {
                std::cerr << "[http] Forwarding scene project=" << projectId.value << " id=" << sceneId.value
                          << " to renderer with " << feeds.size() << " feeds" << std::endl;
            }
            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadSceneDefinition;
            message.commandId = generateCommandId();
            message.loadSceneDefinition = core::LoadSceneDefinitionMessage{*scene, feeds, assets};

            size_t sentCount = rendererRegistry_->broadcastMessage(message);
            if (sentCount == 0) {
                respondWithError(res, 503, "No renderers connected");
                return;
            }
            res.status = 200;
            res.set_content(json({{"status", "sent"}}).dump(), "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost(R"(/projects/([^/]+)/renderer/playCue)",
                 [this](const ::httplib::Request& req, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing project id");
                return;
            }
            core::ProjectId projectId{req.matches[1]};
            auto body = json::parse(req.body);
            if (!body.contains("cueId") || !body["cueId"].is_string()) {
                respondWithError(res, 400, "Missing or invalid cueId");
                return;
            }

            core::CueId cueId{body["cueId"].get<std::string>()};
            auto cue = cueRepository_.findCueById(projectId, cueId);
            if (!cue.has_value()) {
                respondWithError(res, 400, "Cue does not exist");
                return;
            }

            auto scene = sceneRepository_.findSceneById(projectId, cue->getSceneId());
            if (!scene.has_value()) {
                respondWithError(res, 400, "Scene does not exist for cue");
                return;
            }

            std::string cueError;
            if (!core::validateCueForScene(*cue, *scene, cueError)) {
                respondWithError(res, 400, cueError);
                return;
            }

            auto feeds = feedRepository_.listFeeds(projectId);
            std::string sceneError;
            if (!core::validateSceneFeeds(*scene, feeds, sceneError)) {
                respondWithError(res, 400, sceneError);
                return;
            }

            core::Scene sceneWithCue = *scene;
            applyCueToScene(*cue, sceneWithCue);

            std::vector<core::Feed> rendererFeeds;
            std::string feedError;
            if (!collectFeedsForScene(sceneWithCue, rendererFeeds, feedError)) {
                respondWithError(res, 400, feedError);
                return;
            }
            std::vector<core::Asset> assets;
            if (!collectAssetsForFeeds(rendererFeeds, assets, feedError)) {
                respondWithError(res, 400, feedError);
                return;
            }

            if (verbose_) {
                std::cerr << "[http] Forwarding cue project=" << projectId.value << " id=" << cueId.value
                          << " scene=" << sceneWithCue.getId().value << " to renderer with " << rendererFeeds.size()
                          << " feeds" << std::endl;
            }

            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadSceneDefinition;
            message.commandId = generateCommandId();
            message.loadSceneDefinition = core::LoadSceneDefinitionMessage{sceneWithCue, rendererFeeds, assets};

            size_t sentCount = rendererRegistry_->broadcastMessage(message);
            if (sentCount == 0) {
                respondWithError(res, 503, "No renderers connected");
                return;
            }
            res.status = 200;
            res.set_content(json({{"status", "sent"}, {"cueId", cueId.value}, {"sceneId", sceneWithCue.getId().value}})
                                .dump(),
                            "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    // Toggle test pattern (calibration grid) on all connected renderers
    registerPost("/renderer/testPattern", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        try {
            auto body = json::parse(req.body);
            bool enabled = body.value("enabled", true);

            core::RendererMessage message{};
            message.type = core::RendererMessageType::ShowTestPattern;
            message.commandId = generateCommandId();
            message.showTestPattern = core::ShowTestPatternMessage{enabled};

            size_t sentCount = rendererRegistry_->broadcastMessage(message);
            if (sentCount == 0) {
                respondWithError(res, 503, "No renderers connected");
                return;
            }
            res.status = 200;
            res.set_content(json({{"status", "sent"}, {"enabled", enabled}, {"renderers", sentCount}}).dump(),
                            "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    // Show/hide crosshair overlay on renderers (for vertex alignment during drag)
    registerPost("/renderer/crosshair", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        try {
            auto body = json::parse(req.body);
            bool enabled = body.value("enabled", false);
            float x = body.value("x", 0.0f);
            float y = body.value("y", 0.0f);

            core::RendererMessage message{};
            message.type = core::RendererMessageType::ShowCrosshair;
            message.commandId = generateCommandId();
            message.showCrosshair = core::ShowCrosshairMessage{enabled, x, y};

            size_t sentCount = rendererRegistry_->broadcastMessage(message);
            if (sentCount == 0) {
                // Don't error on no renderers - crosshair is a nice-to-have
                res.status = 200;
                res.set_content(json({{"status", "no_renderers"}, {"enabled", enabled}}).dump(),
                                "application/json");
                return;
            }
            res.status = 200;
            res.set_content(json({{"status", "sent"}, {"enabled", enabled}, {"x", x}, {"y", y}, {"renderers", sentCount}}).dump(),
                            "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    if (!webRoot_.empty()) {
        registerStaticRoutes();
    }
}

namespace {
bool hasExtension(const std::string& path) {
    auto dot = path.find_last_of('.');
    auto slash = path.find_last_of('/');
    if (dot == std::string::npos) {
        return false;
    }
    if (slash == std::string::npos) {
        return true;
    }
    return dot > slash;
}

bool isApiPath(const std::string& path) {
    return path.rfind("/api", 0) == 0;
}

bool readFile(const std::filesystem::path& path, std::string& contents) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    return true;
}
}  // namespace

void HttpServer::registerStaticRoutes() {
    std::filesystem::path root(webRoot_);
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        if (verbose_) {
            std::cerr << "[http] Web root not found: " << root.string() << std::endl;
        }
        return;
    }

    if (!server_->set_mount_point("/", root.string())) {
        if (verbose_) {
            std::cerr << "[http] Failed to mount web root: " << root.string() << std::endl;
        }
        return;
    }

    const auto indexPath = (root / "index.html").string();
    server_->set_error_handler([indexPath](const ::httplib::Request& req, ::httplib::Response& res) {
        if (res.status != 404 || req.method != "GET") {
            return;
        }
        if (isApiPath(req.path) || hasExtension(req.path)) {
            return;
        }
        std::string contents;
        if (!readFile(indexPath, contents)) {
            return;
        }
        res.status = 200;
        res.set_content(contents, "text/html");
    });
}

void HttpServer::respondWithError(::httplib::Response& res, int status, const std::string& message) {
    if (verbose_) {
        std::cerr << "[http] error " << status << ": " << message << std::endl;
    }
    res.status = status;
    res.set_content(json({{"error", message}}).dump(), "application/json");
}

std::string HttpServer::generateCommandId() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "cmd-" << now;
    return oss.str();
}

bool HttpServer::collectFeedsForScene(const core::Scene& scene, std::vector<core::Feed>& feeds, std::string& error) {
    std::vector<std::string> feedOrder;
    std::unordered_set<std::string> seenFeedIds;
    for (const auto& surface : scene.getSurfaces()) {
        const auto& feedIdValue = surface.getFeedId().value;
        if (!feedIdValue.empty() && seenFeedIds.insert(feedIdValue).second) {
            feedOrder.push_back(feedIdValue);
        }
    }

    std::unordered_map<std::string, core::Feed> feedsById;
    for (const auto& feed : feedRepository_.listFeeds(scene.getProjectId())) {
        feedsById.emplace(feed.getId().value, feed);
    }

    feeds.clear();
    feeds.reserve(feedOrder.size());
    for (const auto& feedId : feedOrder) {
        auto it = feedsById.find(feedId);
        if (it == feedsById.end()) {
            error = "Feed not found: " + feedId;
            return false;
        }
        feeds.push_back(it->second);
    }

    return true;
}

bool HttpServer::collectAssetsForFeeds(const std::vector<core::Feed>& feeds, std::vector<core::Asset>& assets,
                                       std::string& error) {
    std::unordered_set<std::string> seen;
    for (const auto& feed : feeds) {
        auto asset = assetRepository_.findAssetById(feed.getAssetId());
        if (!asset.has_value()) {
            error = "Feed '" + feed.getId().value + "' references missing asset '" + feed.getAssetId().value + "'.";
            return false;
        }
        if (seen.insert(asset->getId().value).second) {
            assets.push_back(*asset);
        }
    }
    error.clear();
    return true;
}

}  // namespace projection::server::http
