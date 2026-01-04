#pragma once

#include <map>
#include <string>

#include "projection/core/Ids.h"

namespace projection::core {

class Cue {
 public:
  Cue() = default;
  Cue(ProjectId projectId, CueId id, std::string name, SceneId sceneId);

  const ProjectId& getProjectId() const { return projectId_; }
  void setProjectId(const ProjectId& projectId) { projectId_ = projectId; }

  const CueId& getId() const { return id_; }
  void setId(const CueId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const SceneId& getSceneId() const { return sceneId_; }
  void setSceneId(const SceneId& sceneId) { sceneId_ = sceneId; }

  const std::map<SurfaceId, float>& getSurfaceOpacities() const { return surfaceOpacities_; }
  std::map<SurfaceId, float>& getSurfaceOpacities() { return surfaceOpacities_; }

  const std::map<SurfaceId, float>& getSurfaceBrightnesses() const { return surfaceBrightnesses_; }
  std::map<SurfaceId, float>& getSurfaceBrightnesses() { return surfaceBrightnesses_; }

 private:
  ProjectId projectId_{};
  CueId id_{};
  std::string name_{};
  SceneId sceneId_{};
  std::map<SurfaceId, float> surfaceOpacities_{};
  std::map<SurfaceId, float> surfaceBrightnesses_{};
};

}  // namespace projection::core
