#include "db/SchemaMigrations.h"

#include "db/SqliteConnection.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace projection::server::db {

namespace {
const char* kCreateSchemaVersion = R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
)SQL";

const char* kDropOldTables = R"SQL(
DROP TABLE IF EXISTS project_cues;
DROP TABLE IF EXISTS cues;
DROP TABLE IF EXISTS surfaces;
DROP TABLE IF EXISTS scenes;
DROP TABLE IF EXISTS feeds;
DROP TABLE IF EXISTS project_assets;
DROP TABLE IF EXISTS assets;
DROP TABLE IF EXISTS projects;
)SQL";

const char* kCreateProjectsTable = R"SQL(
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    asset_ids_json TEXT NOT NULL,
    scene_ids_json TEXT NOT NULL,
    feed_ids_json TEXT NOT NULL,
    cue_order_json TEXT NOT NULL,
    settings_json TEXT NOT NULL
);
)SQL";

const char* kCreateAssetsTable = R"SQL(
CREATE TABLE IF NOT EXISTS assets (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    path TEXT NOT NULL,
    variants_json TEXT NOT NULL
);
)SQL";

const char* kCreateProjectAssetsTable = R"SQL(
CREATE TABLE IF NOT EXISTS project_assets (
    project_id TEXT NOT NULL,
    asset_id TEXT NOT NULL,
    PRIMARY KEY(project_id, asset_id),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(asset_id) REFERENCES assets(id) ON DELETE CASCADE
);
)SQL";

const char* kCreateFeedsTable = R"SQL(
CREATE TABLE IF NOT EXISTS feeds (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    asset_id TEXT NOT NULL,
    settings_json TEXT NOT NULL,
    PRIMARY KEY(project_id, id),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(asset_id) REFERENCES assets(id)
);
)SQL";

const char* kCreateScenesTable = R"SQL(
CREATE TABLE IF NOT EXISTS scenes (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT,
    settings_json TEXT NOT NULL,
    PRIMARY KEY(project_id, id),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);
)SQL";

const char* kCreateSurfacesTable = R"SQL(
CREATE TABLE IF NOT EXISTS surfaces (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    scene_id TEXT NOT NULL,
    name TEXT NOT NULL,
    feed_id TEXT NOT NULL,
    z_order INTEGER NOT NULL,
    opacity REAL NOT NULL,
    brightness REAL NOT NULL,
    blend_mode TEXT NOT NULL,
    vertices_json TEXT NOT NULL,
    rotation REAL NOT NULL DEFAULT 0,
    PRIMARY KEY(project_id, id),
    FOREIGN KEY(project_id, scene_id) REFERENCES scenes(project_id, id) ON DELETE CASCADE,
    FOREIGN KEY(project_id, feed_id) REFERENCES feeds(project_id, id)
);
)SQL";

const char* kCreateCuesTable = R"SQL(
CREATE TABLE IF NOT EXISTS cues (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    scene_id TEXT NOT NULL,
    surface_opacities_json TEXT NOT NULL,
    surface_brightnesses_json TEXT NOT NULL,
    PRIMARY KEY(project_id, id),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(project_id, scene_id) REFERENCES scenes(project_id, id)
);
)SQL";

void ensureSchemaVersionTable(sqlite3* handle) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, kCreateSchemaVersion, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create schema_version table: " + error);
    }
}

void setSchemaVersion(sqlite3* handle, int version) {
    const char* deleteSql = "DELETE FROM schema_version;";
    sqlite3_exec(handle, deleteSql, nullptr, nullptr, nullptr);

    const char* insertSql = "INSERT INTO schema_version(version) VALUES(?);";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, insertSql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare schema version insert: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_bind_int(stmt, 1, version);
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result != SQLITE_DONE) {
        throw std::runtime_error("Failed to set schema version: " + std::string(sqlite3_errmsg(handle)));
    }
}

void executeSql(sqlite3* handle, const char* sql, const std::string& label) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, sql, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to " + label + ": " + error);
    }
}

void createTables(sqlite3* handle) {
    executeSql(handle, kCreateProjectsTable, "create projects table");
    executeSql(handle, kCreateAssetsTable, "create assets table");
    executeSql(handle, kCreateProjectAssetsTable, "create project_assets table");
    executeSql(handle, kCreateFeedsTable, "create feeds table");
    executeSql(handle, kCreateScenesTable, "create scenes table");
    executeSql(handle, kCreateSurfacesTable, "create surfaces table");
    executeSql(handle, kCreateCuesTable, "create cues table");
}

}  // namespace

void SchemaMigrations::applyMigrations(SqliteConnection& connection) {
    auto lock = connection.lock();
    sqlite3* handle = connection.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    ensureSchemaVersionTable(handle);

    // No backwards compatibility required. Start from a clean schema.
    executeSql(handle, kDropOldTables, "drop existing tables");
    createTables(handle);
    setSchemaVersion(handle, 1);
}

}  // namespace projection::server::db
