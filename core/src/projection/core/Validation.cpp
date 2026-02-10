#include "projection/core/Validation.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace projection::core {

bool validateSurface(const Surface& surface, std::string& errorMessage) {
  if (!surface.isValid()) {
    errorMessage = "Surface '" + surface.getId().value + "' is invalid.";
    return false;
  }

  errorMessage.clear();
  return true;
}

bool validateSceneFeeds(const Scene& scene, const std::vector<Feed>& feeds, std::string& errorMessage) {
  for (const auto& surface : scene.getSurfaces()) {
    if (!validateSurface(surface, errorMessage)) {
      return false;
    }

    const auto& feedId = surface.getFeedId();
    auto feedIt = std::find_if(feeds.begin(), feeds.end(), [&](const Feed& feed) {
      return feed.getId() == feedId && feed.getProjectId() == scene.getProjectId();
    });

    if (feedIt == feeds.end()) {
      errorMessage = "Surface '" + surface.getId().value + "' references unknown feed '" + feedId.value + "'.";
      return false;
    }
  }

  errorMessage.clear();
  return true;
}

bool validateCueForScene(const Cue& cue, const Scene& scene, std::string& errorMessage) {
  if (cue.getProjectId() != scene.getProjectId()) {
    errorMessage = "Cue '" + cue.getId().value + "' targets project '" + cue.getProjectId().value +
                   "' which does not match scene project '" + scene.getProjectId().value + "'.";
    return false;
  }
  if (cue.getSceneId() != scene.getId()) {
    errorMessage = "Cue '" + cue.getId().value + "' targets scene '" + cue.getSceneId().value +
                   "' which does not match scene '" + scene.getId().value + "'.";
    return false;
  }

  auto validateSurfaceReference = [&](const SurfaceId& surfaceId) {
    if (scene.findSurface(surfaceId) == nullptr) {
      errorMessage = "Cue references unknown surface '" + surfaceId.value + "' for scene '" + scene.getId().value + "'.";
      return false;
    }
    return true;
  };

  for (const auto& [surfaceId, _] : cue.getSurfaceOpacities()) {
    if (!validateSurfaceReference(surfaceId)) {
      return false;
    }
  }

  for (const auto& [surfaceId, _] : cue.getSurfaceBrightnesses()) {
    if (!validateSurfaceReference(surfaceId)) {
      return false;
    }
  }

  errorMessage.clear();
  return true;
}

bool validateProjectCues(const Project& project, const std::vector<Asset>& assets, const std::vector<Feed>& feeds,
                         const std::vector<Scene>& scenes, const std::vector<Cue>& cues,
                         std::string& errorMessage) {
  if (project.getId().value.empty()) {
    errorMessage = "Project id must not be empty.";
    return false;
  }

  if (project.getName().empty()) {
    errorMessage = "Project name must not be empty.";
    return false;
  }

  if (project.getCreatedAt().empty() || project.getUpdatedAt().empty()) {
    errorMessage = "Project '" + project.getId().value + "' must include createdAt and updatedAt timestamps.";
    return false;
  }

  std::unordered_map<std::string, const Asset*> assetById;
  for (const auto& asset : assets) {
    assetById.emplace(asset.getId().value, &asset);
  }
  for (const auto& assetId : project.getAssetIds()) {
    if (assetById.find(assetId.value) == assetById.end()) {
      errorMessage = "Project '" + project.getId().value + "' references missing asset '" + assetId.value + "'.";
      return false;
    }
  }

  std::unordered_map<std::string, const Feed*> feedById;
  for (const auto& feed : feeds) {
    feedById.emplace(feed.getId().value, &feed);
  }
  for (const auto& feedId : project.getFeedIds()) {
    auto feedIt = feedById.find(feedId.value);
    if (feedIt == feedById.end()) {
      errorMessage = "Project '" + project.getId().value + "' references missing feed '" + feedId.value + "'.";
      return false;
    }
    if (feedIt->second->getProjectId() != project.getId()) {
      errorMessage = "Project '" + project.getId().value + "' references feed '" + feedId.value +
                     "' with mismatched project id.";
      return false;
    }
  }

  std::unordered_map<std::string, const Scene*> sceneById;
  for (const auto& scene : scenes) {
    sceneById.emplace(scene.getId().value, &scene);
  }
  for (const auto& sceneId : project.getSceneIds()) {
    auto sceneIt = sceneById.find(sceneId.value);
    if (sceneIt == sceneById.end()) {
      errorMessage = "Project '" + project.getId().value + "' references missing scene '" + sceneId.value + "'.";
      return false;
    }
    if (sceneIt->second->getProjectId() != project.getId()) {
      errorMessage = "Project '" + project.getId().value + "' references scene '" + sceneId.value +
                     "' with mismatched project id.";
      return false;
    }
  }

  std::unordered_map<std::string, const Cue*> cueById;
  for (const auto& cue : cues) {
    cueById.emplace(cue.getId().value, &cue);
  }

  for (const auto& cueId : project.getCueOrder()) {
    auto cueIt = cueById.find(cueId.value);
    if (cueIt == cueById.end()) {
      errorMessage = "Project '" + project.getId().value + "' references missing cue '" + cueId.value + "'.";
      return false;
    }
    if (cueIt->second->getProjectId() != project.getId()) {
      errorMessage = "Project '" + project.getId().value + "' references cue '" + cueId.value +
                     "' with mismatched project id.";
      return false;
    }
  }

  for (const auto channel : project.getSettings().midiChannels) {
    if (channel < 1 || channel > 16) {
      errorMessage = "Project '" + project.getId().value + "' has invalid MIDI channel '" + std::to_string(channel) +
                     "'. Expected range 1-16.";
      return false;
    }
  }

  for (const auto& [controller, target] : project.getSettings().controllers) {
    if (controller.empty() || target.empty()) {
      errorMessage = "Project '" + project.getId().value +
                     "' must not contain empty controller names or targets in settings.controllers.";
      return false;
    }
  }

  errorMessage.clear();
  return true;
}

}  // namespace projection::core
