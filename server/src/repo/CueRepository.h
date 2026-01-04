#pragma once

#include <optional>
#include <vector>

#include "db/SqliteConnection.h"
#include "projection/core/Cue.h"

namespace projection::server::repo {

class CueRepository {
public:
    explicit CueRepository(db::SqliteConnection& connection);

    core::Cue createCue(const core::Cue& cue);
    std::vector<core::Cue> listCues(const core::ProjectId& projectId);
    std::optional<core::Cue> findCueById(const core::ProjectId& projectId, const core::CueId& id);
    core::Cue updateCue(const core::Cue& cue);
    void deleteCue(const core::ProjectId& projectId, const core::CueId& id);

private:
    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo
