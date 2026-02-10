#include "RenderState.h"

#include <iostream>
#include <unordered_map>

using projection::core::Asset;
using projection::core::AssetType;
using projection::core::Feed;
using projection::core::Scene;

namespace projection::renderer {
namespace {

std::unordered_map<std::string, Asset> mapAssetsById(const std::vector<Asset>& assets) {
  std::unordered_map<std::string, Asset> mapping;
  mapping.reserve(assets.size());
  for (const auto& asset : assets) {
    mapping.emplace(asset.getId().value, asset);
  }
  return mapping;
}

std::string resolveAssetPath(const Feed& feed, const Asset& asset) {
  const auto& variantPath = feed.getSettings().variantPath;
  if (!variantPath.empty()) {
    return variantPath;
  }
  return asset.getPath();
}

}  // namespace

std::unordered_map<std::string, std::string> mapVideoFeedFilePaths(const Scene& /*scene*/,
                                                                   const std::vector<Feed>& feeds,
                                                                   const std::vector<Asset>& assets) {
  std::unordered_map<std::string, std::string> mapping;
  auto assetMap = mapAssetsById(assets);
  for (const auto& feed : feeds) {
    auto assetIt = assetMap.find(feed.getAssetId().value);
    if (assetIt == assetMap.end()) {
      continue;
    }
    if (assetIt->second.getType() != AssetType::VideoFile) {
      continue;
    }
    mapping.emplace(feed.getId().value, resolveAssetPath(feed, assetIt->second));
  }
  return mapping;
}

void RenderState::loadSceneDefinition(const Scene& scene, const std::vector<Feed>& feeds,
                                      const std::vector<Asset>& assets) {
  currentScene_ = scene;
  currentFeeds_ = feeds;
  currentAssets_ = assets;
  videoFeeds_.clear();
  imageFeeds_.clear();

  auto assetMap = mapAssetsById(assets);
  auto mapping = mapVideoFeedFilePaths(scene, feeds, assets);

  for (const auto& feed : feeds) {
    auto assetIt = assetMap.find(feed.getAssetId().value);
    if (assetIt == assetMap.end()) {
      std::cerr << "[RenderState] Missing asset for feed " << feed.getId().value << std::endl;
      continue;
    }

    const auto& asset = assetIt->second;
    const auto resolvedPath = resolveAssetPath(feed, asset);

    if (asset.getType() == AssetType::VideoFile) {
      auto it = mapping.find(feed.getId().value);
      if (it == mapping.end()) {
        continue;
      }

      VideoFeedResource resource{feed.getId(), {}};
      const bool loaded = resource.player.load(it->second);
      if (loaded) {
        resource.player.setLoopState(OF_LOOP_NORMAL);
        resource.player.play();
      }
      resource.filePath = it->second;

      videoFeeds_.emplace(feed.getId().value, std::move(resource));
    } else if (asset.getType() == AssetType::ImageFile) {
      std::cerr << "[RenderState] Loading image feed: " << feed.getId().value
                << " path=" << resolvedPath << std::endl;

      ImageFeedResource resource;
      resource.id = feed.getId();
      resource.filePath = resolvedPath;
      resource.panDirection = feed.getSettings().panDirection;
      resource.panDurationSeconds = feed.getSettings().panDurationSeconds;
      resource.visiblePortion = feed.getSettings().visiblePortion;
      resource.panStartTime = ofGetElapsedTimef();
      resource.pingPongReverse = false;

      if (resource.image.load(resolvedPath)) {
        std::cerr << "[RenderState] Image loaded successfully: " << resolvedPath
                  << " (" << resource.image.getWidth() << "x" << resource.image.getHeight() << ")" << std::endl;
        imageFeeds_.emplace(feed.getId().value, std::move(resource));
      } else {
        std::cerr << "[RenderState] Failed to load image: " << resolvedPath << std::endl;
      }
    }
  }
}

void RenderState::updateVideoPlayers() {
  for (auto& entry : videoFeeds_) {
    entry.second.player.update();
  }
}

}  // namespace projection::renderer

