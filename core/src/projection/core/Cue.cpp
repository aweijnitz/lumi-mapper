#include "projection/core/Cue.h"

#include <utility>

namespace projection::core {

Cue::Cue(ProjectId projectId, CueId id, std::string name, SceneId sceneId)
    : projectId_(std::move(projectId)),
      id_(std::move(id)),
      name_(std::move(name)),
      sceneId_(std::move(sceneId)) {}

}  // namespace projection::core
