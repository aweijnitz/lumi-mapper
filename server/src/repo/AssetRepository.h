#pragma once

#include <optional>
#include <vector>

#include "db/SqliteConnection.h"
#include "projection/core/Asset.h"

namespace projection::server::repo {

class AssetRepository {
public:
    explicit AssetRepository(db::SqliteConnection& connection);

    core::Asset createAsset(const core::Asset& asset);
    std::vector<core::Asset> listAssets();
    std::vector<core::Asset> listAssetsForProject(const core::ProjectId& projectId);
    std::optional<core::Asset> findAssetById(const core::AssetId& assetId);
    core::Asset updateAsset(const core::Asset& asset);
    void deleteAsset(const core::AssetId& assetId);

    void addAssetToProject(const core::ProjectId& projectId, const core::AssetId& assetId);
    void removeAssetFromProject(const core::ProjectId& projectId, const core::AssetId& assetId);
    std::vector<core::AssetId> listAssetIdsForProject(const core::ProjectId& projectId);

private:
    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo

