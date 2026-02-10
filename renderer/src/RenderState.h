#pragma once

#include <unordered_map>
#include <vector>

#include <ofMain.h>

#include <projection/core/Asset.h>
#include <projection/core/Feed.h>
#include <projection/core/Scene.h>

namespace projection::renderer {

struct VideoFeedResource {
  projection::core::FeedId id;
  ofVideoPlayer player;
  std::string filePath;
};

struct ImageFeedResource {
  projection::core::FeedId id;
  ofImage image;
  std::string filePath;
  projection::core::PanDirection panDirection = projection::core::PanDirection::LeftToRight;
  float panDurationSeconds = 120.0f;
  float visiblePortion = 0.6f;
  float panStartTime = 0.0f;      // Time when pan animation started
  bool pingPongReverse = false;   // For ping-pong mode: current direction
};

// Extracts the configured file paths for video feeds.
std::unordered_map<std::string, std::string> mapVideoFeedFilePaths(
    const projection::core::Scene& scene, const std::vector<projection::core::Feed>& feeds,
    const std::vector<projection::core::Asset>& assets);

class RenderState {
 public:
  RenderState() = default;

  void loadSceneDefinition(const projection::core::Scene& scene,
                           const std::vector<projection::core::Feed>& feeds,
                           const std::vector<projection::core::Asset>& assets);
  void updateVideoPlayers();

  const projection::core::Scene& currentScene() const { return currentScene_; }
  const std::vector<projection::core::Feed>& currentFeeds() const { return currentFeeds_; }
  const std::vector<projection::core::Asset>& currentAssets() const { return currentAssets_; }
  const std::unordered_map<std::string, VideoFeedResource>& videoFeeds() const { return videoFeeds_; }
  const std::unordered_map<std::string, ImageFeedResource>& imageFeeds() const { return imageFeeds_; }
  std::unordered_map<std::string, ImageFeedResource>& imageFeeds() { return imageFeeds_; }

 private:
  projection::core::Scene currentScene_{};
  std::vector<projection::core::Feed> currentFeeds_{};
  std::vector<projection::core::Asset> currentAssets_{};
  std::unordered_map<std::string, VideoFeedResource> videoFeeds_{};
  std::unordered_map<std::string, ImageFeedResource> imageFeeds_{};
};

}  // namespace projection::renderer
