#include "projection/core/Feed.h"

namespace projection::core {

Feed::Feed(ProjectId projectId, FeedId id, std::string name, AssetId assetId, FeedSettings settings)
    : projectId_(std::move(projectId)),
      id_(std::move(id)),
      name_(std::move(name)),
      assetId_(std::move(assetId)),
      settings_(std::move(settings)) {}

Feed makeFeed(const ProjectId& projectId, const FeedId& id, const std::string& name, const AssetId& assetId,
              const FeedSettings& settings) {
  return Feed(projectId, id, name, assetId, settings);
}

}  // namespace projection::core

