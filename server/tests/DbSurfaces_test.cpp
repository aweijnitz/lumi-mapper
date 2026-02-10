#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sqlite3.h>
#include <string>

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;

namespace {
std::filesystem::path tempDb(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}
}

TEST_CASE("SchemaMigrations creates surfaces table", "[db][migrations][surfaces]") {
    auto dbPath = tempDb("projection_mapper_surfaces.sqlite");
    std::filesystem::remove(dbPath);

    SqliteConnection connection;
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    sqlite3* handle = connection.getHandle();
    REQUIRE(handle != nullptr);

    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle,
                              "INSERT INTO projects(id, name, description, created_at, updated_at, asset_ids_json, "
                              "scene_ids_json, feed_ids_json, cue_order_json, settings_json) "
                              "VALUES('proj-1', 'Project', 'desc', '2026-02-02T10:00:00Z', "
                              "'2026-02-02T10:00:00Z', '[]', '[]', '[]', '[]', '{}');",
                              nullptr, nullptr, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    result = sqlite3_exec(handle,
                          "INSERT INTO assets(id, name, type, path, variants_json) "
                          "VALUES('asset-1', 'Feed', 'VideoFile', '/media/a.mp4', '[]');",
                          nullptr, nullptr, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    result = sqlite3_exec(handle,
                          "INSERT INTO feeds(project_id, id, name, asset_id, settings_json) "
                          "VALUES('proj-1', 'f1', 'Feed', 'asset-1', '{}');",
                          nullptr, nullptr, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    result = sqlite3_exec(handle,
                          "INSERT INTO scenes(project_id, id, name, description, settings_json) "
                          "VALUES('proj-1', 'scene-1', 'Scene', 'desc', '{}');",
                          nullptr, nullptr, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    result = sqlite3_exec(handle,
                          "INSERT INTO surfaces(project_id, id, scene_id, name, feed_id, z_order, opacity, brightness, "
                          "blend_mode, vertices_json, rotation) VALUES('proj-1', 's1', 'scene-1', 'Surf', 'f1', 0, "
                          "1.0, 1.0, 'Normal', '[]', 0);",
                          nullptr, nullptr, &errorMessage);
    std::string errorStr = errorMessage ? errorMessage : "";
    sqlite3_free(errorMessage);

    REQUIRE(result == SQLITE_OK);
}

