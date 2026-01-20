#pragma once

#include <string>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

namespace projection::core {

struct VideoFileConfig {
  std::string filePath;
};

// Configuration for image feeds with pan animation
// The pan effect shows a portion of the image and slowly pans across it
struct ImageFileConfig {
  std::string filePath;
  PanDirection panDirection = PanDirection::LeftToRight;  // Direction of the pan
  float panDurationSeconds = 120.0f;                      // Duration for one full pan sweep (default 2 minutes)
  float visiblePortion = 0.6f;                            // What fraction of image width is visible (0.5-1.0)
};

class Feed {
 public:
  Feed() = default;
  Feed(ProjectId projectId, FeedId id, std::string name, FeedType type, std::string configJson);

  const ProjectId& getProjectId() const { return projectId_; }
  void setProjectId(const ProjectId& projectId) { projectId_ = projectId; }

  const FeedId& getId() const { return id_; }
  void setId(const FeedId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  FeedType getType() const { return type_; }
  void setType(FeedType type) { type_ = type; }

  const std::string& getConfigJson() const { return configJson_; }
  void setConfigJson(const std::string& json) { configJson_ = json; }

  bool operator==(const Feed& other) const {
    return projectId_ == other.projectId_ && id_ == other.id_ && name_ == other.name_ && type_ == other.type_ &&
           configJson_ == other.configJson_;
  }

 private:
  ProjectId projectId_{};
  FeedId id_{};
  std::string name_{};
  FeedType type_{FeedType::VideoFile};
  std::string configJson_{};
};

VideoFileConfig parseVideoFileConfig(const Feed& feed);
Feed makeVideoFileFeed(const ProjectId& projectId, const FeedId& id, const std::string& name,
                       const std::string& filePath);

ImageFileConfig parseImageFileConfig(const Feed& feed);
Feed makeImageFileFeed(const ProjectId& projectId, const FeedId& id, const std::string& name,
                       const ImageFileConfig& config);

}  // namespace projection::core
