#include "http/HttpServer.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "projection/core/Serialization.h"
#include "projection/core/RendererProtocol.h"
#include "projection/core/Validation.h"
#include "projection/core/Project.h"

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

std::string assetTypeForPath(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    // Convert to lowercase for case-insensitive comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".mkv" || ext == ".webm" || ext == ".m4v") {
        return "video";
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp" || ext == ".bmp" ||
        ext == ".tiff" || ext == ".tif") {
        return "image";
    }
    return "unknown";
}

std::optional<std::string> sanitizeAssetName(const std::string& raw) {
    std::filesystem::path candidate(raw);
    const auto name = candidate.filename().string();
    if (name.empty() || name == "." || name == "..") {
        return std::nullopt;
    }
    return name;
}
}  // namespace

HttpServer::HttpServer(repo::FeedRepository& feedRepository, repo::SceneRepository& sceneRepository,
                       repo::CueRepository& cueRepository, repo::ProjectRepository& projectRepository,
                       std::shared_ptr<renderer::RendererRegistry> rendererRegistry, bool verbose,
                       std::string webRoot)
    : feedRepository_(feedRepository),
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

    registerPost(R"(/projects/([^/]+)/feeds)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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
            auto created = feedRepository_.createFeed(feed);
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

    registerPut(R"(/projects/([^/]+)/feeds/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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
            feed.setId(core::FeedId{req.matches[2]});
            auto updated = feedRepository_.updateFeed(feed);
            res.status = 200;
            res.set_content(json(updated).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/feeds/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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

    registerPost(R"(/projects/([^/]+)/scenes)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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
            if (verbose_) {
                std::cerr << "[http] Created scene id=" << created.getId().value << std::endl;
            }
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerPut(R"(/projects/([^/]+)/scenes/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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
            res.status = 200;
            res.set_content(json(updated).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    registerDelete(R"(/projects/([^/]+)/scenes/(.+))",
                   [this](const ::httplib::Request& req, ::httplib::Response& res) {
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

    registerPost(R"(/projects/([^/]+)/cues)", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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

    registerPut(R"(/projects/([^/]+)/cues/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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

    registerDelete(R"(/projects/([^/]+)/cues/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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
            auto cues = cueRepository_.listCues(project.getId());
            std::string error;
            if (!core::validateProjectCues(project, cues, error)) {
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
            auto cues = cueRepository_.listCues(project.getId());
            std::string error;
            if (!core::validateProjectCues(project, cues, error)) {
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
            auto root = findAssetsRoot();
            if (!root.has_value()) {
                respondWithError(res, 404, "Assets directory not found.");
                return;
            }
            json payload = json::array();
            for (const auto& entry : std::filesystem::directory_iterator(*root)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto path = entry.path();
                const auto name = path.filename().string();
                json asset{{"id", name}, {"name", name}, {"path", path.string()}, {"type", assetTypeForPath(path)}};
                payload.push_back(asset);
            }
            res.status = 200;
            res.set_content(payload.dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost("/assets", [this](const ::httplib::Request& req, ::httplib::Response& res) {
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

            json asset{{"id", *safeName},
                       {"name", *safeName},
                       {"path", targetPath.string()},
                       {"type", assetTypeForPath(targetPath)}};
            res.status = 201;
            res.set_content(asset.dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerDelete(R"(/assets/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing asset name");
                return;
            }
            const auto safeName = sanitizeAssetName(std::string(req.matches[1]));
            if (!safeName.has_value()) {
                respondWithError(res, 400, "Invalid asset filename.");
                return;
            }

            auto root = findAssetsRoot();
            if (!root.has_value()) {
                respondWithError(res, 404, "Assets directory not found.");
                return;
            }

            const auto targetPath = *root / *safeName;
            if (!std::filesystem::exists(targetPath)) {
                respondWithError(res, 404, "Asset not found.");
                return;
            }

            std::filesystem::remove(targetPath);
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

            if (verbose_) {
                std::cerr << "[http] Forwarding scene project=" << projectId.value << " id=" << sceneId.value
                          << " to renderer with " << feeds.size() << " feeds" << std::endl;
            }
            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadSceneDefinition;
            message.commandId = generateCommandId();
            message.loadSceneDefinition = core::LoadSceneDefinitionMessage{*scene, feeds};

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

            if (verbose_) {
                std::cerr << "[http] Forwarding cue project=" << projectId.value << " id=" << cueId.value
                          << " scene=" << sceneWithCue.getId().value << " to renderer with " << rendererFeeds.size()
                          << " feeds" << std::endl;
            }

            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadSceneDefinition;
            message.commandId = generateCommandId();
            message.loadSceneDefinition = core::LoadSceneDefinitionMessage{sceneWithCue, rendererFeeds};

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

    registerPost("/demo/two-video-test", [this](const ::httplib::Request&, ::httplib::Response& res) {
        if (!rendererRegistry_) {
            respondWithError(res, 500, "Renderer registry not configured");
            return;
        }

        try {
            const auto suffix = generateCommandId();
            const auto projectId = core::ProjectId{"demo-" + suffix};

            auto findAssetPath = [](const std::string& filename) -> std::optional<std::filesystem::path> {
                std::vector<std::filesystem::path> candidates = {
                    std::filesystem::current_path() / "data" / "assets" / filename,
                    std::filesystem::current_path().parent_path() / "data" / "assets" / filename,
                    std::filesystem::current_path().parent_path().parent_path() / "data" / "assets" / filename};
#ifdef PROJECTION_MAPPER_ROOT_DIR
                candidates.emplace_back(std::filesystem::path(PROJECTION_MAPPER_ROOT_DIR) / "data" / "assets" / filename);
#endif
                for (const auto& candidate : candidates) {
                    if (std::filesystem::exists(candidate)) {
                        return std::filesystem::weakly_canonical(candidate);
                    }
                }
                return std::nullopt;
            };

            const auto clipAPath = findAssetPath("clipA.mp4");
            const auto clipBPath = findAssetPath("clipB.mp4");
            if (!clipAPath.has_value() || !clipBPath.has_value()) {
                respondWithError(res, 500, "Demo assets not found under data/assets (expected clipA.mp4 and clipB.mp4)");
                return;
            }

            core::Project demoProject(projectId, "Demo Project " + suffix, "Auto-generated demo project", {},
                                      core::ProjectSettings{});
            projectRepository_.createProject(demoProject);

            nlohmann::json clipAConfig{{"filePath", clipAPath->string()}};
            nlohmann::json clipBConfig{{"filePath", clipBPath->string()}};

            core::Feed feedA(projectId, core::FeedId{}, "Demo Clip A", core::FeedType::VideoFile, clipAConfig.dump());
            core::Feed feedB(projectId, core::FeedId{}, "Demo Clip B", core::FeedType::VideoFile, clipBConfig.dump());

            feedA = feedRepository_.createFeed(feedA);
            feedB = feedRepository_.createFeed(feedB);

            std::vector<core::Vec2> quadA{{-0.8f, -0.6f}, {-0.1f, -0.5f}, {-0.1f, 0.2f}, {-0.8f, 0.1f}};
            std::vector<core::Vec2> quadB{{0.1f, -0.3f}, {0.8f, -0.2f}, {0.7f, 0.5f}, {0.0f, 0.4f}};

            core::Surface surfaceA(core::SurfaceId{"demo-surface-a-" + suffix}, "Demo Surface A", quadA, feedA.getId());
            core::Surface surfaceB(core::SurfaceId{"demo-surface-b-" + suffix}, "Demo Surface B", quadB, feedB.getId());

            core::Scene scene(projectId, core::SceneId{}, "Two Video Demo Scene", "Auto-generated demo scene",
                              std::vector<core::Surface>{surfaceA, surfaceB});

            auto feeds = feedRepository_.listFeeds(projectId);
            std::string validationError;
            if (!core::validateSceneFeeds(scene, feeds, validationError)) {
                respondWithError(res, 400, validationError);
                return;
            }

            auto createdScene = sceneRepository_.createScene(scene);

            std::vector<core::Feed> rendererFeeds;
            std::string error;
            if (!collectFeedsForScene(createdScene, rendererFeeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

            if (verbose_) {
                std::cerr << "[http] Demo endpoint created project " << projectId.value << " scene "
                          << createdScene.getId().value << " with feeds " << feedA.getId().value << ","
                          << feedB.getId().value << " -> sending to renderer" << std::endl;
            }
            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadSceneDefinition;
            message.commandId = generateCommandId();
            message.loadSceneDefinition = core::LoadSceneDefinitionMessage{createdScene, rendererFeeds};

            size_t sentCount = rendererRegistry_->broadcastMessage(message);
            if (sentCount == 0) {
                respondWithError(res, 503, "No renderers connected");
                return;
            }

            json payload{{"projectId", projectId.value},
                         {"sceneId", createdScene.getId().value},
                         {"feedIds", json::array({feedA.getId().value, feedB.getId().value})},
                         {"surfaceIds", json::array({surfaceA.getId().value, surfaceB.getId().value})}};
            res.status = 200;
            res.set_content(payload.dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    registerPost("/demo/clear-projects", [this](const ::httplib::Request&, ::httplib::Response& res) {
        try {
            const auto projects = projectRepository_.listProjects();
            std::size_t deleted = 0;
            for (const auto& project : projects) {
                if (project.getId().value.rfind("demo-", 0) == 0) {
                    projectRepository_.deleteProject(project.getId());
                    ++deleted;
                }
            }
            res.status = 200;
            res.set_content(json({{"deletedProjects", deleted}}).dump(), "application/json");
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

}  // namespace projection::server::http
