#include "projection/core/Asset.h"

namespace projection::core {

Asset::Asset(AssetId id, std::string name, AssetType type, std::string path, std::vector<AssetVariant> variants)
    : id_(std::move(id)),
      name_(std::move(name)),
      type_(type),
      path_(std::move(path)),
      variants_(std::move(variants)) {}

}  // namespace projection::core

