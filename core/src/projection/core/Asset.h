#pragma once

#include <string>
#include <vector>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

namespace projection::core {

struct AssetVariant {
  std::string path;
  std::string note;

  bool operator==(const AssetVariant& other) const { return path == other.path && note == other.note; }
};

class Asset {
 public:
  Asset() = default;
  Asset(AssetId id, std::string name, AssetType type, std::string path, std::vector<AssetVariant> variants = {});

  const AssetId& getId() const { return id_; }
  void setId(const AssetId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  AssetType getType() const { return type_; }
  void setType(AssetType type) { type_ = type; }

  const std::string& getPath() const { return path_; }
  void setPath(const std::string& path) { path_ = path; }

  const std::vector<AssetVariant>& getVariants() const { return variants_; }
  std::vector<AssetVariant>& getVariants() { return variants_; }
  void setVariants(const std::vector<AssetVariant>& variants) { variants_ = variants; }

  bool operator==(const Asset& other) const {
    return id_ == other.id_ && name_ == other.name_ && type_ == other.type_ && path_ == other.path_ &&
           variants_ == other.variants_;
  }

 private:
  AssetId id_{};
  std::string name_{};
  AssetType type_{AssetType::VideoFile};
  std::string path_{};
  std::vector<AssetVariant> variants_{};
};

}  // namespace projection::core

