#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Cue.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using projection::core::Cue;
using projection::core::ProjectId;
using projection::core::Project;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::makeCueId;
using projection::core::makeProjectId;
using projection::core::makeSceneId;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::repo::CueRepository;
using projection::server::repo::ProjectRepository;
using projection::server::repo::SceneRepository;

namespace {
std::filesystem::path tempDb(const std::string& name) { return std::filesystem::temp_directory_path() / name; }

void requireParseFailure(const std::function<void()>& action, const std::string& expectedProjectId,
                         const std::string& expectedField) {
    bool didThrow = false;
    try {
        action();
    } catch (const std::runtime_error& ex) {
        didThrow = true;
        const std::string message = ex.what();
        REQUIRE(message.find(expectedProjectId) != std::string::npos);
        REQUIRE(message.find(expectedField) != std::string::npos);
    }
    REQUIRE(didThrow);
}
}  // namespace

TEST_CASE("ProjectRepository persists and retrieves ordered cues", "[repo][project]") {
    SqliteConnection connection;
    auto dbPath = tempDb("project_repo.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    SceneRepository sceneRepo(connection);
    CueRepository cueRepo(connection);
    ProjectRepository projectRepo(connection);

    ProjectSettings settings;
    settings.midiChannels = {1};
    settings.controllers["fader1"] = "master";
    Project project{makeProjectId("proj-1"),
                    "Show",
                    "Demo",
                    "2026-02-02T10:00:00Z",
                    "2026-02-02T10:00:00Z",
                    {},
                    {},
                    {},
                    {},
                    settings};
    projectRepo.createProject(project);

    Scene scene{project.getId(), makeSceneId("1"), "Scene", "desc", {}};
    sceneRepo.createScene(scene);

    Cue cueA{project.getId(), makeCueId("cue-A"), "A", scene.getId()};
    Cue cueB{project.getId(), makeCueId("cue-B"), "B", scene.getId()};
    cueRepo.createCue(cueA);
    cueRepo.createCue(cueB);

    Project updated = project;
    updated.getCueOrder() = {cueA.getId(), cueB.getId()};
    projectRepo.updateProject(updated);

    auto projects = projectRepo.listProjects();
    REQUIRE(projects.size() == 1);
    REQUIRE(projects.front().getCueOrder().size() == 2);
    REQUIRE(projects.front().getCueOrder()[0] == cueA.getId());
    REQUIRE(projects.front().getSettings().controllers.at("fader1") == "master");

    updated.setDescription("Updated");
    updated.getCueOrder() = {cueB.getId()};
    auto updatedSettings = updated.getSettings();
    updatedSettings.midiChannels = {2, 3};
    updated.setSettings(updatedSettings);
    projectRepo.updateProject(updated);

    auto fetched = projectRepo.findProjectById(project.getId());
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getDescription() == "Updated");
    REQUIRE(fetched->getCueOrder().size() == 1);
    REQUIRE(fetched->getCueOrder()[0] == cueB.getId());
    std::vector<int> updatedChannels{2, 3};
    REQUIRE(fetched->getSettings().midiChannels == updatedChannels);
}

TEST_CASE("ProjectRepository tolerates empty settings_json values", "[repo][project]") {
    SqliteConnection connection;
    auto dbPath = tempDb("project_repo_empty_settings.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    connection.execute(
        "INSERT INTO projects(id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, "
        "feed_ids_json, cue_order_json, settings_json) VALUES('proj-empty', 'Empty', 'No settings', "
        "'2026-02-02T10:00:00Z', '2026-02-02T10:00:00Z', '[]', '[]', '[]', '[]', '');");

    ProjectRepository projectRepo(connection);
    auto projects = projectRepo.listProjects();
    REQUIRE(projects.size() == 1);
    REQUIRE(projects.front().getSettings().controllers.empty());
    REQUIRE(projects.front().getSettings().midiChannels.empty());
    REQUIRE(projects.front().getSettings().globalConfig.empty());

    auto fetched = projectRepo.findProjectById(makeProjectId("proj-empty"));
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getSettings().controllers.empty());
    REQUIRE(fetched->getSettings().midiChannels.empty());
    REQUIRE(fetched->getSettings().globalConfig.empty());
}

TEST_CASE("ProjectRepository surfaces malformed settings_json content", "[repo][project][error]") {
    SqliteConnection connection;
    auto dbPath = tempDb("project_repo_bad_settings.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    connection.execute(
        "INSERT INTO projects(id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, "
        "feed_ids_json, cue_order_json, settings_json) VALUES('proj-bad-settings', 'Broken', 'Bad settings', "
        "'2026-02-02T10:00:00Z', '2026-02-02T10:00:00Z', '[]', '[]', '[]', '[]', '{\"controllers\":');");

    ProjectRepository projectRepo(connection);

    requireParseFailure([&]() { static_cast<void>(projectRepo.listProjects()); }, "proj-bad-settings",
                        "settings_json");
    requireParseFailure([&]() { static_cast<void>(projectRepo.findProjectById(makeProjectId("proj-bad-settings"))); },
                        "proj-bad-settings", "settings_json");
}

TEST_CASE("ProjectRepository rejects corrupt project id arrays", "[repo][project][error]") {
    SqliteConnection connection;
    auto dbPath = tempDb("project_repo_bad_arrays.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    connection.execute(
        "INSERT INTO projects(id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, "
        "feed_ids_json, cue_order_json, settings_json) VALUES('proj-bad-array', 'Broken', 'Bad ids', "
        "'2026-02-02T10:00:00Z', '2026-02-02T10:00:00Z', '[]', '[]', '[]', '[\"cue-a\", 7]', '{}');");

    ProjectRepository projectRepo(connection);

    requireParseFailure([&]() { static_cast<void>(projectRepo.listProjects()); }, "proj-bad-array",
                        "cue_order_json");
    requireParseFailure([&]() { static_cast<void>(projectRepo.findProjectById(makeProjectId("proj-bad-array"))); },
                        "proj-bad-array", "cue_order_json");
}
