#include "projection/core/Feed.h"

#include <nlohmann/json.hpp>
#include <utility>
#include <stdexcept>

namespace projection::core {

Feed::Feed(ProjectId projectId, FeedId id, std::string name, FeedType type, std::string configJson)
    : projectId_(std::move(projectId)),
      id_(std::move(id)),
      name_(std::move(name)),
      type_(type),
      configJson_(std::move(configJson)) {}

VideoFileConfig parseVideoFileConfig(const Feed& feed) {
  if (feed.getType() != FeedType::VideoFile) {
    throw std::runtime_error("parseVideoFileConfig requires a VideoFile feed");
  }

  auto json = nlohmann::json::parse(feed.getConfigJson());
  if (!json.is_object() || !json.contains("filePath") || !json["filePath"].is_string()) {
    throw std::runtime_error("Invalid VideoFile feed config: missing filePath");
  }

  return VideoFileConfig{json["filePath"].get<std::string>()};
}

Feed makeVideoFileFeed(const ProjectId& projectId, const FeedId& id, const std::string& name,
                       const std::string& filePath) {
  nlohmann::json config{{"filePath", filePath}};
  return Feed(projectId, id, name, FeedType::VideoFile, config.dump());
}

ImageFileConfig parseImageFileConfig(const Feed& feed) {
  if (feed.getType() != FeedType::ImageFile) {
    throw std::runtime_error("parseImageFileConfig requires an ImageFile feed");
  }

  auto json = nlohmann::json::parse(feed.getConfigJson());
  if (!json.is_object() || !json.contains("filePath") || !json["filePath"].is_string()) {
    throw std::runtime_error("Invalid ImageFile feed config: missing filePath");
  }

  ImageFileConfig config;
  config.filePath = json["filePath"].get<std::string>();

  // Parse optional pan settings with defaults
  if (json.contains("panDirection") && json["panDirection"].is_string()) {
    PanDirection dir;
    if (fromString(json["panDirection"].get<std::string>(), dir)) {
      config.panDirection = dir;
    }
  }

  if (json.contains("panDurationSeconds") && json["panDurationSeconds"].is_number()) {
    config.panDurationSeconds = json["panDurationSeconds"].get<float>();
  }

  if (json.contains("visiblePortion") && json["visiblePortion"].is_number()) {
    config.visiblePortion = std::clamp(json["visiblePortion"].get<float>(), 0.3f, 1.0f);
  }

  return config;
}

Feed makeImageFileFeed(const ProjectId& projectId, const FeedId& id, const std::string& name,
                       const ImageFileConfig& config) {
  nlohmann::json jsonConfig{
      {"filePath", config.filePath},
      {"panDirection", toString(config.panDirection)},
      {"panDurationSeconds", config.panDurationSeconds},
      {"visiblePortion", config.visiblePortion}};
  return Feed(projectId, id, name, FeedType::ImageFile, jsonConfig.dump());
}

}  // namespace projection::core
