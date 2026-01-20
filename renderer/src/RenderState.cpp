#include "RenderState.h"

#include <iostream>
#include <projection/core/Feed.h>

using projection::core::Feed;
using projection::core::FeedType;
using projection::core::ImageFileConfig;
using projection::core::Scene;
using projection::core::VideoFileConfig;
using projection::core::parseImageFileConfig;
using projection::core::parseVideoFileConfig;

namespace projection::renderer {

std::unordered_map<std::string, std::string> mapVideoFeedFilePaths(const Scene& /*scene*/,
                                                                   const std::vector<Feed>& feeds) {
  std::unordered_map<std::string, std::string> mapping;
  for (const auto& feed : feeds) {
    if (feed.getType() != FeedType::VideoFile) {
      continue;
    }
    VideoFileConfig config = parseVideoFileConfig(feed);
    mapping.emplace(feed.getId().value, config.filePath);
  }
  return mapping;
}

void RenderState::loadSceneDefinition(const Scene& scene, const std::vector<Feed>& feeds) {
  currentScene_ = scene;
  currentFeeds_ = feeds;
  videoFeeds_.clear();
  imageFeeds_.clear();

  auto mapping = mapVideoFeedFilePaths(scene, feeds);
  for (const auto& feed : feeds) {
    if (feed.getType() == FeedType::VideoFile) {
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
    } else if (feed.getType() == FeedType::ImageFile) {
      try {
        ImageFileConfig config = parseImageFileConfig(feed);
        std::cerr << "[RenderState] Loading image feed: " << feed.getId().value
                  << " path=" << config.filePath << std::endl;

        ImageFeedResource resource;
        resource.id = feed.getId();
        resource.filePath = config.filePath;
        resource.panDirection = config.panDirection;
        resource.panDurationSeconds = config.panDurationSeconds;
        resource.visiblePortion = config.visiblePortion;
        resource.panStartTime = ofGetElapsedTimef();  // Start pan animation from now
        resource.pingPongReverse = false;

        if (resource.image.load(config.filePath)) {
          std::cerr << "[RenderState] Image loaded successfully: " << config.filePath
                    << " (" << resource.image.getWidth() << "x" << resource.image.getHeight() << ")" << std::endl;
          imageFeeds_.emplace(feed.getId().value, std::move(resource));
        } else {
          std::cerr << "[RenderState] Failed to load image: " << config.filePath << std::endl;
        }
      } catch (const std::exception& ex) {
        std::cerr << "[RenderState] Error loading image feed " << feed.getId().value
                  << ": " << ex.what() << std::endl;
      } catch (...) {
        std::cerr << "[RenderState] Unknown error loading image feed " << feed.getId().value << std::endl;
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
