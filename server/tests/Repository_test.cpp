#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Asset.h"
#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"
#include "repo/AssetRepository.h"
#include "repo/CueRepository.h"
#include "repo/FeedRepository.h"
#include "repo/ProjectRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using projection::core::Asset;
using projection::core::AssetId;
using projection::core::AssetType;
using projection::core::AssetVariant;
using projection::core::Cue;
using projection::core::Feed;
using projection::core::Project;
using projection::core::ProjectId;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::makeAssetId;
using projection::core::makeCueId;
using projection::core::makeProjectId;
using projection::core::makeFeedId;
using projection::core::makeSceneId;
using projection::core::makeSurfaceId;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::repo::AssetRepository;
using projection::server::repo::CueRepository;
using projection::server::repo::FeedRepository;
using projection::server::repo::ProjectRepository;
using projection::server::repo::SceneRepository;

namespace {
std::filesystem::path createTempDbPath(const std::string& name) {
    auto tempDir = std::filesystem::temp_directory_path();
    return tempDir / name;
}

void setupTestDb(SqliteConnection& connection, const std::string& filename) {
    auto dbPath = createTempDbPath(filename);
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);
}

bool expectRuntimeError(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

Project createProject(ProjectRepository& repo, const ProjectId& projectId) {
    ProjectSettings settings;
    Project project(projectId, "Test Project", "", "2026-02-02T10:00:00Z", "2026-02-02T10:00:00Z", {}, {}, {}, {},
                    settings);
    return repo.createProject(project);
}
}

TEST_CASE("FeedRepository creates and lists feeds", "[repo][feed]") {
    SqliteConnection connection;
    setupTestDb(connection, "feed_repo.sqlite");

    AssetRepository assetRepo(connection);
    FeedRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));

    auto asset = assetRepo.createAsset(Asset{AssetId{}, "Clip", AssetType::VideoFile, "/media/video.mp4"});

    Feed feedWithoutId(project.getId(), makeFeedId(""), "Test Feed", asset.getId());
    Feed created = repo.createFeed(feedWithoutId);
    REQUIRE(!created.getId().value.empty());
    REQUIRE(created.getName() == "Test Feed");

    Feed explicitIdFeed(project.getId(), makeFeedId("42"), "Second", asset.getId());
    Feed createdWithId = repo.createFeed(explicitIdFeed);
    REQUIRE(createdWithId.getId().value == "42");

    auto feeds = repo.listFeeds(project.getId());
    REQUIRE(feeds.size() == 2);
    REQUIRE(std::any_of(feeds.begin(), feeds.end(),
                        [&](const Feed& f) { return f.getId() == created.getId() && f.getName() == "Test Feed"; }));
    auto fetched = repo.findFeedById(project.getId(), makeFeedId("42"));
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getAssetId() == asset.getId());
}

TEST_CASE("SceneRepository creates and lists scenes", "[repo][scene]") {
    SqliteConnection connection;
    setupTestDb(connection, "scene_repo.sqlite");

    SceneRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));

    Scene sceneNoId(project.getId(), makeSceneId(""), "My Scene", "First scene", {});
    Scene created = repo.createScene(sceneNoId);
    REQUIRE(!created.getId().value.empty());

    Scene sceneWithId(project.getId(), makeSceneId("7"), "Another", "More", {});
    Scene createdWithId = repo.createScene(sceneWithId);
    REQUIRE(createdWithId.getId().value == "7");

    auto scenes = repo.listScenes(project.getId());
    REQUIRE(scenes.size() == 2);
    auto fetchedGenerated = repo.findSceneById(project.getId(), created.getId());
    REQUIRE(fetchedGenerated.has_value());
    REQUIRE(fetchedGenerated->getName() == "My Scene");

    auto fetchedExplicit = repo.findSceneById(project.getId(), makeSceneId("7"));
    REQUIRE(fetchedExplicit.has_value());
    REQUIRE(fetchedExplicit->getDescription() == "More");
    REQUIRE(fetchedExplicit->getSurfaces().empty());
}

TEST_CASE("FeedRepository accepts string ids", "[repo][feed]") {
    SqliteConnection connection;
    setupTestDb(connection, "feed_repo_error.sqlite");

    AssetRepository assetRepo(connection);
    FeedRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));
    auto asset = assetRepo.createAsset(Asset{AssetId{}, "Clip", AssetType::VideoFile, "/media/video.mp4"});
    Feed customId(project.getId(), makeFeedId("abc-123"), "Custom", asset.getId());

    Feed created = repo.createFeed(customId);
    REQUIRE(created.getId().value == "abc-123");
    auto found = repo.findFeedById(project.getId(), makeFeedId("abc-123"));
    REQUIRE(found.has_value());
}

TEST_CASE("AssetRepository persists variants and project associations", "[repo][asset]") {
    SqliteConnection connection;
    setupTestDb(connection, "asset_repo.sqlite");

    AssetRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));

    Asset asset(makeAssetId("asset-1"), "Clip", AssetType::VideoFile, "/media/video.mp4",
                {AssetVariant{"/media/video-proxy.mp4", "proxy"}, AssetVariant{"/media/video-loop.mp4", "loop"}});
    auto created = repo.createAsset(asset);

    repo.addAssetToProject(project.getId(), created.getId());
    repo.addAssetToProject(project.getId(), created.getId());

    auto assetIds = repo.listAssetIdsForProject(project.getId());
    REQUIRE(assetIds.size() == 1);
    REQUIRE(assetIds.front() == created.getId());

    auto projectAssets = repo.listAssetsForProject(project.getId());
    REQUIRE(projectAssets.size() == 1);
    REQUIRE(projectAssets.front().getVariants() == asset.getVariants());

    created.setName("Clip Updated");
    created.setPath("/media/video-updated.mp4");
    created.setVariants({AssetVariant{"/media/video-hd.mp4", "hd"}});
    repo.updateAsset(created);

    auto fetched = repo.findAssetById(created.getId());
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getName() == "Clip Updated");
    REQUIRE(fetched->getPath() == "/media/video-updated.mp4");
    REQUIRE(fetched->getVariants() == created.getVariants());

    repo.removeAssetFromProject(project.getId(), created.getId());
    REQUIRE(repo.listAssetIdsForProject(project.getId()).empty());
    REQUIRE(repo.listAssetsForProject(project.getId()).empty());
}

TEST_CASE("CueRepository persists cue overrides across read paths", "[repo][cue]") {
    SqliteConnection connection;
    setupTestDb(connection, "cue_repo.sqlite");

    ProjectRepository projectRepo(connection);
    SceneRepository sceneRepo(connection);
    CueRepository cueRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));

    auto sceneA = sceneRepo.createScene(Scene(project.getId(), makeSceneId("scene-a"), "Scene A", "desc", {}));
    auto sceneB = sceneRepo.createScene(Scene(project.getId(), makeSceneId("scene-b"), "Scene B", "desc", {}));

    Cue cue(project.getId(), makeCueId("cue-1"), "Intro", sceneA.getId());
    cue.getSurfaceOpacities().emplace(makeSurfaceId("surface-1"), 0.25F);
    cue.getSurfaceOpacities().emplace(makeSurfaceId("surface-2"), 0.75F);
    cue.getSurfaceBrightnesses().emplace(makeSurfaceId("surface-1"), 0.5F);
    cue.getSurfaceBrightnesses().emplace(makeSurfaceId("surface-2"), 0.9F);
    cueRepo.createCue(cue);

    auto listed = cueRepo.listCues(project.getId());
    REQUIRE(listed.size() == 1);
    REQUIRE(listed.front().getSurfaceOpacities() == cue.getSurfaceOpacities());
    REQUIRE(listed.front().getSurfaceBrightnesses() == cue.getSurfaceBrightnesses());

    auto fetched = cueRepo.findCueById(project.getId(), cue.getId());
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getName() == "Intro");
    REQUIRE(fetched->getSceneId() == sceneA.getId());
    REQUIRE(fetched->getSurfaceOpacities() == cue.getSurfaceOpacities());
    REQUIRE(fetched->getSurfaceBrightnesses() == cue.getSurfaceBrightnesses());

    cue.setName("Updated Intro");
    cue.setSceneId(sceneB.getId());
    cue.getSurfaceOpacities()[makeSurfaceId("surface-1")] = 0.1F;
    cue.getSurfaceBrightnesses().erase(makeSurfaceId("surface-2"));
    cue.getSurfaceBrightnesses()[makeSurfaceId("surface-3")] = 1.0F;
    cueRepo.updateCue(cue);

    auto updated = cueRepo.findCueById(project.getId(), cue.getId());
    REQUIRE(updated.has_value());
    REQUIRE(updated->getName() == "Updated Intro");
    REQUIRE(updated->getSceneId() == sceneB.getId());
    REQUIRE(updated->getSurfaceOpacities() == cue.getSurfaceOpacities());
    REQUIRE(updated->getSurfaceBrightnesses() == cue.getSurfaceBrightnesses());

    cueRepo.deleteCue(project.getId(), cue.getId());
    REQUIRE(cueRepo.listCues(project.getId()).empty());
    REQUIRE(!cueRepo.findCueById(project.getId(), cue.getId()).has_value());
}

TEST_CASE("SceneRepository prevents duplicate numeric ids", "[repo][scene][error]") {
    SqliteConnection connection;
    setupTestDb(connection, "scene_repo_error.sqlite");

    SceneRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));
    Scene sceneA(project.getId(), makeSceneId("5"), "Primary", "desc", {});
    Scene sceneB(project.getId(), makeSceneId("5"), "Duplicate", "desc", {});

    repo.createScene(sceneA);
    REQUIRE(expectRuntimeError([&]() { repo.createScene(sceneB); }));
}
