#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"
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

using projection::core::Feed;
using projection::core::FeedType;
using projection::core::Project;
using projection::core::ProjectId;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::makeProjectId;
using projection::core::makeFeedId;
using projection::core::makeSceneId;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
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
    Project project(projectId, "Test Project", "", {}, settings);
    return repo.createProject(project);
}
}

TEST_CASE("FeedRepository creates and lists feeds", "[repo][feed]") {
    SqliteConnection connection;
    setupTestDb(connection, "feed_repo.sqlite");

    FeedRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));

    Feed feedWithoutId(project.getId(), makeFeedId(""), "Test Feed", FeedType::VideoFile,
                       R"({\"path\": \"video.mp4\"})");
    Feed created = repo.createFeed(feedWithoutId);
    REQUIRE(!created.getId().value.empty());
    REQUIRE(created.getName() == "Test Feed");

    Feed explicitIdFeed(project.getId(), makeFeedId("42"), "Second", FeedType::Camera, "{}");
    Feed createdWithId = repo.createFeed(explicitIdFeed);
    REQUIRE(createdWithId.getId().value == "42");

    auto feeds = repo.listFeeds(project.getId());
    REQUIRE(feeds.size() == 2);
    REQUIRE(std::any_of(feeds.begin(), feeds.end(),
                        [&](const Feed& f) { return f.getId() == created.getId() && f.getName() == "Test Feed"; }));
    auto fetched = repo.findFeedById(project.getId(), makeFeedId("42"));
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getType() == FeedType::Camera);
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

    FeedRepository repo(connection);
    ProjectRepository projectRepo(connection);
    auto project = createProject(projectRepo, makeProjectId("proj-1"));
    Feed customId(project.getId(), makeFeedId("abc-123"), "Custom", FeedType::Generated, "{}");

    Feed created = repo.createFeed(customId);
    REQUIRE(created.getId().value == "abc-123");
    auto found = repo.findFeedById(project.getId(), makeFeedId("abc-123"));
    REQUIRE(found.has_value());
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
