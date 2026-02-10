#include "projection/core/Project.h"

namespace projection::core {

Project::Project(ProjectId id, std::string name, std::string description, std::string createdAt,
                 std::string updatedAt, std::vector<AssetId> assetIds, std::vector<SceneId> sceneIds,
                 std::vector<FeedId> feedIds, std::vector<CueId> cueOrder, ProjectSettings settings)
    : id_(std::move(id)),
      name_(std::move(name)),
      description_(std::move(description)),
      createdAt_(std::move(createdAt)),
      updatedAt_(std::move(updatedAt)),
      assetIds_(std::move(assetIds)),
      sceneIds_(std::move(sceneIds)),
      feedIds_(std::move(feedIds)),
      cueOrder_(std::move(cueOrder)),
      settings_(std::move(settings)) {}

}  // namespace projection::core

