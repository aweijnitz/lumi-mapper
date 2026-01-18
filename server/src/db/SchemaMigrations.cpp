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

const char* kCreateProjectsTable = R"SQL(
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    settings_json TEXT NOT NULL
);
)SQL";

const char* kCreateFeedsTable = R"SQL(
CREATE TABLE IF NOT EXISTS feeds (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    config_json TEXT NOT NULL,
    PRIMARY KEY(project_id, id),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);
)SQL";

const char* kCreateScenesTable = R"SQL(
CREATE TABLE IF NOT EXISTS scenes (
    project_id TEXT NOT NULL,
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT,
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

const char* kCreateProjectCuesTable = R"SQL(
CREATE TABLE IF NOT EXISTS project_cues (
    project_id TEXT NOT NULL,
    cue_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    PRIMARY KEY(project_id, position),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(project_id, cue_id) REFERENCES cues(project_id, id),
    UNIQUE(project_id, cue_id)
);
)SQL";

// Migration: Add rotation column to surfaces table (defaults to 0)
const char* kMigrationAddRotationColumn = R"SQL(
ALTER TABLE surfaces ADD COLUMN rotation REAL NOT NULL DEFAULT 0;
)SQL";

int getSchemaVersion(sqlite3* handle) {
    const char* sql = "SELECT version FROM schema_version LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        return 0;
    }
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

void setSchemaVersion(sqlite3* handle, int version) {
    // Clear existing version and insert new one
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

bool columnExists(sqlite3* handle, const char* table, const char* column) {
    std::string sql = "PRAGMA table_info(" + std::string(table) + ");";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        return false;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* colName = sqlite3_column_text(stmt, 1);
        if (colName && std::string(reinterpret_cast<const char*>(colName)) == column) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

void ensureSchemaVersionTable(sqlite3* handle) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, kCreateSchemaVersion, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create schema_version table: " + error);
    }
}

void createTables(sqlite3* handle) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, kCreateProjectsTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create projects table: " + error);
    }

    result = sqlite3_exec(handle, kCreateFeedsTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create feeds table: " + error);
    }

    result = sqlite3_exec(handle, kCreateScenesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create scenes table: " + error);
    }

    result = sqlite3_exec(handle, kCreateSurfacesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create surfaces table: " + error);
    }

    result = sqlite3_exec(handle, kCreateCuesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create cues table: " + error);
    }

    result = sqlite3_exec(handle, kCreateProjectCuesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create project_cues table: " + error);
    }
}
}  // namespace

void SchemaMigrations::applyMigrations(SqliteConnection& connection) {
    auto lock = connection.lock();
    sqlite3* handle = connection.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    ensureSchemaVersionTable(handle);
    createTables(handle);

    // Apply incremental migrations based on schema version
    int currentVersion = getSchemaVersion(handle);

    // Migration 1: Add rotation column to surfaces table
    if (currentVersion < 1) {
        // Only run ALTER if column doesn't already exist (for safety)
        if (!columnExists(handle, "surfaces", "rotation")) {
            char* errorMessage = nullptr;
            int result = sqlite3_exec(handle, kMigrationAddRotationColumn, nullptr, nullptr, &errorMessage);
            if (result != SQLITE_OK) {
                std::string error = errorMessage ? errorMessage : "Unknown error";
                sqlite3_free(errorMessage);
                throw std::runtime_error("Failed to add rotation column: " + error);
            }
        }
        setSchemaVersion(handle, 1);
    }
}

}  // namespace projection::server::db
