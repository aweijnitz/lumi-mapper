#pragma once

#include <string>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

namespace projection::core {

// Project-specific settings for a feed (effects + asset variant selection).
struct FeedSettings {
  std::string variantPath;                                // Optional override path for asset variant
  bool monochrome = false;                                // Optional monochrome filter
  PanDirection panDirection = PanDirection::LeftToRight;  // Direction of image pan
  float panDurationSeconds = 120.0f;                      // Duration for one full pan sweep (default 2 minutes)
  float visiblePortion = 0.6f;                            // Fraction of image width visible (0.5-1.0)

  bool operator==(const FeedSettings& other) const {
    return variantPath == other.variantPath && monochrome == other.monochrome &&
           panDirection == other.panDirection && panDurationSeconds == other.panDurationSeconds &&
           visiblePortion == other.visiblePortion;
  }
};

class Feed {
 public:
  Feed() = default;
  Feed(ProjectId projectId, FeedId id, std::string name, AssetId assetId, FeedSettings settings = {});

  const ProjectId& getProjectId() const { return projectId_; }
  void setProjectId(const ProjectId& projectId) { projectId_ = projectId; }

  const FeedId& getId() const { return id_; }
  void setId(const FeedId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const AssetId& getAssetId() const { return assetId_; }
  void setAssetId(const AssetId& assetId) { assetId_ = assetId; }

  const FeedSettings& getSettings() const { return settings_; }
  FeedSettings& getSettings() { return settings_; }
  void setSettings(const FeedSettings& settings) { settings_ = settings; }

  bool operator==(const Feed& other) const {
    return projectId_ == other.projectId_ && id_ == other.id_ && name_ == other.name_ && assetId_ == other.assetId_ &&
           settings_ == other.settings_;
  }

 private:
  ProjectId projectId_{};
  FeedId id_{};
  std::string name_{};
  AssetId assetId_{};
  FeedSettings settings_{};
};

Feed makeFeed(const ProjectId& projectId, const FeedId& id, const std::string& name, const AssetId& assetId,
              const FeedSettings& settings = {});

}  // namespace projection::core
