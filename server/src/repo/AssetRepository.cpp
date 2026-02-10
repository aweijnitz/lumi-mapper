#include "repo/AssetRepository.h"

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
    return "asset-" + std::to_string(now);
}

core::Asset parseAssetRow(const unsigned char* idText, const unsigned char* nameText, const unsigned char* typeText,
                          const unsigned char* pathText, const unsigned char* variantsText) {
    std::string id = idText ? reinterpret_cast<const char*>(idText) : "";
    std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
    std::string typeStr = typeText ? reinterpret_cast<const char*>(typeText) : "";
    std::string path = pathText ? reinterpret_cast<const char*>(pathText) : "";
    std::string variantsJson = variantsText ? reinterpret_cast<const char*>(variantsText) : "[]";

    core::AssetType assetType;
    if (!core::fromString(typeStr, assetType)) {
        throw std::runtime_error("Failed to parse asset type from database: " + typeStr);
    }

    std::vector<core::AssetVariant> variants;
    if (!variantsJson.empty()) {
        try {
            variants = nlohmann::json::parse(variantsJson).get<std::vector<core::AssetVariant>>();
        } catch (...) {
            variants = {};
        }
    }

    return core::Asset(core::AssetId{id}, name, assetType, path, variants);
}
}

AssetRepository::AssetRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Asset AssetRepository::createAsset(const core::Asset& asset) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const std::string idValue = asset.getId().value.empty() ? generateId() : asset.getId().value;
    const char* sql = "INSERT INTO assets(id, name, type, path, variants_json) VALUES(?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    const std::string variantsJson = nlohmann::json(asset.getVariants()).dump();
    const std::string typeString = core::toString(asset.getType());

    result = sqlite3_bind_text(stmt, 1, idValue.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, asset.getName().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 3, typeString.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 4, asset.getPath().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 5, variantsJson.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind asset fields: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert asset: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    core::Asset created = asset;
    created.setId(core::AssetId(idValue));
    return created;
}

std::vector<core::Asset> AssetRepository::listAssets() {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, type, path, variants_json FROM assets ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Asset> assets;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        assets.emplace_back(parseAssetRow(sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
                                          sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                                          sqlite3_column_text(stmt, 4)));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read assets: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return assets;
}

std::vector<core::Asset> AssetRepository::listAssetsForProject(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql =
        "SELECT a.id, a.name, a.type, a.path, a.variants_json "
        "FROM assets a JOIN project_assets pa ON a.id = pa.asset_id "
        "WHERE pa.project_id=? ORDER BY a.id;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project assets select: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for asset list: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Asset> assets;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        assets.emplace_back(parseAssetRow(sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
                                          sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                                          sqlite3_column_text(stmt, 4)));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read project assets: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return assets;
}

std::optional<core::Asset> AssetRepository::findAssetById(const core::AssetId& assetId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, type, path, variants_json FROM assets WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, assetId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind asset id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        auto asset = parseAssetRow(sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1),
                                   sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                                   sqlite3_column_text(stmt, 4));
        sqlite3_finalize(stmt);
        return asset;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Asset AssetRepository::updateAsset(const core::Asset& asset) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (asset.getId().value.empty()) {
        throw std::runtime_error("Asset id must not be empty for update");
    }

    const char* sql = "UPDATE assets SET name=?, type=?, path=?, variants_json=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset update statement: " + std::string(sqlite3_errmsg(handle)));
    }

    const std::string variantsJson = nlohmann::json(asset.getVariants()).dump();
    const std::string typeString = core::toString(asset.getType());

    result = sqlite3_bind_text(stmt, 1, asset.getName().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, typeString.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 3, asset.getPath().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 4, variantsJson.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 5, asset.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind asset update fields: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update asset: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return asset;
}

void AssetRepository::deleteAsset(const core::AssetId& assetId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM assets WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset delete statement: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_bind_text(stmt, 1, assetId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind asset id for delete: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete asset: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

void AssetRepository::addAssetToProject(const core::ProjectId& projectId, const core::AssetId& assetId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "INSERT OR IGNORE INTO project_assets(project_id, asset_id) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_assets insert: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, assetId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project_assets insert fields: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert project asset: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

void AssetRepository::removeAssetFromProject(const core::ProjectId& projectId, const core::AssetId& assetId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM project_assets WHERE project_id=? AND asset_id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_assets delete: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, assetId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project_assets delete fields: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete project asset: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

std::vector<core::AssetId> AssetRepository::listAssetIdsForProject(const core::ProjectId& projectId) {
    auto lock = connection_.lock();
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT asset_id FROM project_assets WHERE project_id=? ORDER BY asset_id;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_assets select: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for asset list: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::AssetId> ids;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* assetIdText = sqlite3_column_text(stmt, 0);
        std::string assetId = assetIdText ? reinterpret_cast<const char*>(assetIdText) : "";
        ids.emplace_back(assetId);
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read project assets: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return ids;
}

}  // namespace projection::server::repo

