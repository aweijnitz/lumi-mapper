#include "repo/ProjectRepository.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "projection/core/Serialization.h"

namespace projection::server::repo {

namespace {
core::ProjectSettings parseSettingsJson(const unsigned char* settingsText, const std::string& projectId) {
    std::string settings = settingsText ? reinterpret_cast<const char*>(settingsText) : "";
    if (settings.empty()) {
        return {};
    }
    try {
        auto settingsJson = nlohmann::json::parse(settings);
        return settingsJson.get<core::ProjectSettings>();
    } catch (const std::exception& ex) {
        std::cerr << "[repo] Failed to parse settings_json for project '" << projectId << "': " << ex.what()
                  << std::endl;
        return {};
    }
}

std::vector<std::string> parseStringArray(const unsigned char* jsonText) {
    std::string raw = jsonText ? reinterpret_cast<const char*>(jsonText) : "[]";
    if (raw.empty()) {
        raw = "[]";
    }
    auto parsed = nlohmann::json::parse(raw);
    if (!parsed.is_array()) {
        return {};
    }
    std::vector<std::string> values;
    for (const auto& entry : parsed) {
        if (entry.is_string()) {
            values.push_back(entry.get<std::string>());
        }
    }
    return values;
}

nlohmann::json toStringArray(const std::vector<std::string>& values) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& value : values) {
        arr.push_back(value);
    }
    return arr;
}
}

ProjectRepository::ProjectRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Project ProjectRepository::createProject(const core::Project& project) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (project.getId().value.empty()) {
        throw std::runtime_error("Project id must not be empty");
    }

    const auto settingsJson = nlohmann::json(project.getSettings()).dump();

    std::vector<std::string> assetIds;
    for (const auto& id : project.getAssetIds()) assetIds.push_back(id.value);
    std::vector<std::string> sceneIds;
    for (const auto& id : project.getSceneIds()) sceneIds.push_back(id.value);
    std::vector<std::string> feedIds;
    for (const auto& id : project.getFeedIds()) feedIds.push_back(id.value);
    std::vector<std::string> cueOrder;
    for (const auto& id : project.getCueOrder()) cueOrder.push_back(id.value);

    const auto assetIdsJson = toStringArray(assetIds).dump();
    const auto sceneIdsJson = toStringArray(sceneIds).dump();
    const auto feedIdsJson = toStringArray(feedIds).dump();
    const auto cueOrderJson = toStringArray(cueOrder).dump();

    const char* sql =
        "INSERT INTO projects(id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, "
        "feed_ids_json, cue_order_json, settings_json) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project insert: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, project.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, project.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, project.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, project.getCreatedAt().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 5, project.getUpdatedAt().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 6, assetIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 7, sceneIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 8, feedIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 9, cueOrderJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 10, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project fields: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    return project;
}

std::vector<core::Project> ProjectRepository::listProjects() {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql =
        "SELECT id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, feed_ids_json, "
        "cue_order_json, settings_json FROM projects ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project select: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Project> projects;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);
        const unsigned char* createdText = sqlite3_column_text(stmt, 3);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 4);
        const unsigned char* assetIdsText = sqlite3_column_text(stmt, 5);
        const unsigned char* sceneIdsText = sqlite3_column_text(stmt, 6);
        const unsigned char* feedIdsText = sqlite3_column_text(stmt, 7);
        const unsigned char* cueOrderText = sqlite3_column_text(stmt, 8);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 9);

        std::string id = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        std::string createdAt = createdText ? reinterpret_cast<const char*>(createdText) : "";
        std::string updatedAt = updatedText ? reinterpret_cast<const char*>(updatedText) : "";

        core::ProjectSettings settings = parseSettingsJson(settingsText, id);

        auto assetIdsRaw = parseStringArray(assetIdsText);
        auto sceneIdsRaw = parseStringArray(sceneIdsText);
        auto feedIdsRaw = parseStringArray(feedIdsText);
        auto cueOrderRaw = parseStringArray(cueOrderText);

        std::vector<core::AssetId> assetIds;
        for (const auto& value : assetIdsRaw) assetIds.emplace_back(value);
        std::vector<core::SceneId> sceneIds;
        for (const auto& value : sceneIdsRaw) sceneIds.emplace_back(value);
        std::vector<core::FeedId> feedIds;
        for (const auto& value : feedIdsRaw) feedIds.emplace_back(value);
        std::vector<core::CueId> cueOrder;
        for (const auto& value : cueOrderRaw) cueOrder.emplace_back(value);

        projects.emplace_back(core::Project(core::ProjectId{id}, name, description, createdAt, updatedAt, assetIds,
                                            sceneIds, feedIds, cueOrder, settings));
    }
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read projects: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    return projects;
}

std::optional<core::Project> ProjectRepository::findProjectById(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql =
        "SELECT id, name, description, created_at, updated_at, asset_ids_json, scene_ids_json, feed_ids_json, "
        "cue_order_json, settings_json FROM projects WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project select by id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);
        const unsigned char* createdText = sqlite3_column_text(stmt, 3);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 4);
        const unsigned char* assetIdsText = sqlite3_column_text(stmt, 5);
        const unsigned char* sceneIdsText = sqlite3_column_text(stmt, 6);
        const unsigned char* feedIdsText = sqlite3_column_text(stmt, 7);
        const unsigned char* cueOrderText = sqlite3_column_text(stmt, 8);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 9);

        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        std::string createdAt = createdText ? reinterpret_cast<const char*>(createdText) : "";
        std::string updatedAt = updatedText ? reinterpret_cast<const char*>(updatedText) : "";
        auto assetIdsRaw = parseStringArray(assetIdsText);
        auto sceneIdsRaw = parseStringArray(sceneIdsText);
        auto feedIdsRaw = parseStringArray(feedIdsText);
        auto cueOrderRaw = parseStringArray(cueOrderText);
        core::ProjectSettings settings = parseSettingsJson(settingsText, projectId.value);
        sqlite3_finalize(stmt);

        std::vector<core::AssetId> assetIds;
        for (const auto& value : assetIdsRaw) assetIds.emplace_back(value);
        std::vector<core::SceneId> sceneIds;
        for (const auto& value : sceneIdsRaw) sceneIds.emplace_back(value);
        std::vector<core::FeedId> feedIds;
        for (const auto& value : feedIdsRaw) feedIds.emplace_back(value);
        std::vector<core::CueId> cueOrder;
        for (const auto& value : cueOrderRaw) cueOrder.emplace_back(value);

        return core::Project(projectId, name, description, createdAt, updatedAt, assetIds, sceneIds, feedIds, cueOrder,
                             settings);
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Project ProjectRepository::updateProject(const core::Project& project) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (project.getId().value.empty()) {
        throw std::runtime_error("Project id must not be empty for update");
    }

    const auto settingsJson = nlohmann::json(project.getSettings()).dump();

    std::vector<std::string> assetIds;
    for (const auto& id : project.getAssetIds()) assetIds.push_back(id.value);
    std::vector<std::string> sceneIds;
    for (const auto& id : project.getSceneIds()) sceneIds.push_back(id.value);
    std::vector<std::string> feedIds;
    for (const auto& id : project.getFeedIds()) feedIds.push_back(id.value);
    std::vector<std::string> cueOrder;
    for (const auto& id : project.getCueOrder()) cueOrder.push_back(id.value);

    const auto assetIdsJson = toStringArray(assetIds).dump();
    const auto sceneIdsJson = toStringArray(sceneIds).dump();
    const auto feedIdsJson = toStringArray(feedIds).dump();
    const auto cueOrderJson = toStringArray(cueOrder).dump();

    const char* sql =
        "UPDATE projects SET name=?, description=?, created_at=?, updated_at=?, asset_ids_json=?, scene_ids_json=?, "
        "feed_ids_json=?, cue_order_json=?, settings_json=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project update: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, project.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, project.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, project.getCreatedAt().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, project.getUpdatedAt().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 5, assetIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 6, sceneIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 7, feedIdsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 8, cueOrderJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 9, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 10, project.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project update fields: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return project;
}

void ProjectRepository::deleteProject(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM projects WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

bool ProjectRepository::projectExists(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "SELECT 1 FROM projects WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project exists query: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for exists query: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    bool exists = rc == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

}  // namespace projection::server::repo
