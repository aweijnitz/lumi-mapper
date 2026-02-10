#include "repo/FeedRepository.h"

#include <sqlite3.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "projection/core/Serialization.h"

namespace projection::server::repo {

namespace {
std::string generateId() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "feed-" + std::to_string(now);
}
}

FeedRepository::FeedRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Feed FeedRepository::createFeed(const core::Feed& feed) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (feed.getProjectId().value.empty()) {
        throw std::runtime_error("Feed project id must not be empty");
    }

    const std::string idValue = feed.getId().value.empty() ? generateId() : feed.getId().value;
    const char* sql = "INSERT INTO feeds(project_id, id, name, asset_id, settings_json) VALUES(?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    const std::string settingsJson = nlohmann::json(feed.getSettings()).dump();

    int bindIndex = 1;
    result = sqlite3_bind_text(stmt, bindIndex++, feed.getProjectId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, idValue.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, feed.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed name: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, feed.getAssetId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed asset id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed settings: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert feed: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);

    core::Feed created = feed;
    created.setId(core::FeedId(idValue));
    return created;
}

std::vector<core::Feed> FeedRepository::listFeeds(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, asset_id, settings_json FROM feeds WHERE project_id=? ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for feed list: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Feed> feeds;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* assetIdText = sqlite3_column_text(stmt, 2);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 3);

        std::string idString = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string assetId = assetIdText ? reinterpret_cast<const char*>(assetIdText) : "";
        std::string settingsJson = settingsText ? reinterpret_cast<const char*>(settingsText) : "";

        core::FeedSettings settings{};
        if (!settingsJson.empty()) {
            try {
                settings = nlohmann::json::parse(settingsJson).get<core::FeedSettings>();
            } catch (...) {
                settings = core::FeedSettings{};
            }
        }

        feeds.emplace_back(core::Feed(projectId, core::FeedId(idString), name, core::AssetId(assetId), settings));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read feeds: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return feeds;
}

std::optional<core::Feed> FeedRepository::findFeedById(const core::ProjectId& projectId, const core::FeedId& feedId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, asset_id, settings_json FROM feeds WHERE project_id=? AND id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed project id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 2, feedId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* assetIdText = sqlite3_column_text(stmt, 2);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 3);

        std::string idString = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string assetId = assetIdText ? reinterpret_cast<const char*>(assetIdText) : "";
        std::string settingsJson = settingsText ? reinterpret_cast<const char*>(settingsText) : "";

        core::FeedSettings settings{};
        if (!settingsJson.empty()) {
            try {
                settings = nlohmann::json::parse(settingsJson).get<core::FeedSettings>();
            } catch (...) {
                settings = core::FeedSettings{};
            }
        }

        sqlite3_finalize(stmt);
        return core::Feed(projectId, core::FeedId(idString), name, core::AssetId(assetId), settings);
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Feed FeedRepository::updateFeed(const core::Feed& feed) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (feed.getId().value.empty()) {
        throw std::runtime_error("Feed id must not be empty for update");
    }
    if (feed.getProjectId().value.empty()) {
        throw std::runtime_error("Feed project id must not be empty for update");
    }

    const char* sql = "UPDATE feeds SET name=?, asset_id=?, settings_json=? WHERE project_id=? AND id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed update statement: " + std::string(sqlite3_errmsg(handle)));
    }

    const std::string settingsJson = nlohmann::json(feed.getSettings()).dump();

    result = sqlite3_bind_text(stmt, 1, feed.getName().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, feed.getAssetId().value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 3, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 4, feed.getProjectId().value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 5, feed.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed update fields: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update feed: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return feed;
}

void FeedRepository::deleteFeed(const core::ProjectId& projectId, const core::FeedId& id) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM feeds WHERE project_id=? AND id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed delete statement: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, id.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id for delete: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete feed: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

}  // namespace projection::server::repo

