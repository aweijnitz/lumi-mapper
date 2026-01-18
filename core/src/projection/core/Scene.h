#pragma once

#include <string>
#include <vector>

#include "projection/core/Feed.h"
#include "projection/core/Ids.h"
#include "projection/core/Surface.h"

namespace projection::core {

// Available scene-level filter types
enum class SceneFilter {
  None,              // No filter applied
  ColorTint,         // Apply distinct color tint to each surface for visual distinction
  Monochrome,        // Convert entire scene to grayscale
  // Future filters can be added here
};

// Scene-level settings for visual effects and rendering options
struct SceneSettings {
  SceneFilter filter = SceneFilter::None;  // Active filter for the scene
  int colorPaletteIndex = 0;               // Which color palette to use for ColorTint filter (0-based)

  bool operator==(const SceneSettings& other) const {
    return filter == other.filter && colorPaletteIndex == other.colorPaletteIndex;
  }
};

class Scene {
 public:
  Scene() = default;
  Scene(ProjectId projectId, SceneId id, std::string name, std::string description, std::vector<Surface> surfaces);

  const ProjectId& getProjectId() const { return projectId_; }
  void setProjectId(const ProjectId& projectId) { projectId_ = projectId; }

  const SceneId& getId() const { return id_; }
  void setId(const SceneId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& getDescription() const { return description_; }
  void setDescription(const std::string& description) { description_ = description; }

  const std::vector<Surface>& getSurfaces() const { return surfaces_; }
  std::vector<Surface>& getSurfaces() { return surfaces_; }
  void setSurfaces(const std::vector<Surface>& surfaces) { surfaces_ = surfaces; }

  const SceneSettings& getSettings() const { return settings_; }
  void setSettings(const SceneSettings& settings) { settings_ = settings; }

  const Surface* findSurface(const SurfaceId& id) const;
  Surface* findSurface(const SurfaceId& id);

  bool isConsistent(const std::vector<Feed>& feeds) const;

  bool operator==(const Scene& other) const {
    return projectId_ == other.projectId_ && id_ == other.id_ && name_ == other.name_ &&
           description_ == other.description_ && surfaces_ == other.surfaces_ &&
           settings_ == other.settings_;
  }

 private:
  ProjectId projectId_{};
  SceneId id_{};
  std::string name_{};
  std::string description_{};
  std::vector<Surface> surfaces_{};
  SceneSettings settings_{};
};

}  // namespace projection::core
