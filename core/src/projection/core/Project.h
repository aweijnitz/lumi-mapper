#pragma once

#include <map>
#include <string>
#include <vector>

#include "projection/core/Ids.h"

namespace projection::core {

struct ProjectSettings {
  std::map<std::string, std::string> controllers{};
  std::vector<int> midiChannels{};
  std::map<std::string, std::string> globalConfig{};
};

class Project {
 public:
  Project() = default;
  Project(ProjectId id, std::string name, std::string description, std::string createdAt, std::string updatedAt,
          std::vector<AssetId> assetIds, std::vector<SceneId> sceneIds, std::vector<FeedId> feedIds,
          std::vector<CueId> cueOrder, ProjectSettings settings = {});

  const ProjectId& getId() const { return id_; }
  void setId(const ProjectId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& getDescription() const { return description_; }
  void setDescription(const std::string& description) { description_ = description; }

  const std::string& getCreatedAt() const { return createdAt_; }
  void setCreatedAt(const std::string& createdAt) { createdAt_ = createdAt; }

  const std::string& getUpdatedAt() const { return updatedAt_; }
  void setUpdatedAt(const std::string& updatedAt) { updatedAt_ = updatedAt; }

  const std::vector<AssetId>& getAssetIds() const { return assetIds_; }
  std::vector<AssetId>& getAssetIds() { return assetIds_; }
  void setAssetIds(const std::vector<AssetId>& assetIds) { assetIds_ = assetIds; }

  const std::vector<SceneId>& getSceneIds() const { return sceneIds_; }
  std::vector<SceneId>& getSceneIds() { return sceneIds_; }
  void setSceneIds(const std::vector<SceneId>& sceneIds) { sceneIds_ = sceneIds; }

  const std::vector<FeedId>& getFeedIds() const { return feedIds_; }
  std::vector<FeedId>& getFeedIds() { return feedIds_; }
  void setFeedIds(const std::vector<FeedId>& feedIds) { feedIds_ = feedIds; }

  const std::vector<CueId>& getCueOrder() const { return cueOrder_; }
  std::vector<CueId>& getCueOrder() { return cueOrder_; }
  void setCueOrder(const std::vector<CueId>& cueOrder) { cueOrder_ = cueOrder; }

  const ProjectSettings& getSettings() const { return settings_; }
  ProjectSettings& getSettings() { return settings_; }
  void setSettings(const ProjectSettings& settings) { settings_ = settings; }

 private:
  ProjectId id_{};
  std::string name_{};
  std::string description_{};
  std::string createdAt_{};
  std::string updatedAt_{};
  std::vector<AssetId> assetIds_{};
  std::vector<SceneId> sceneIds_{};
  std::vector<FeedId> feedIds_{};
  std::vector<CueId> cueOrder_{};
  ProjectSettings settings_{};
};

}  // namespace projection::core
