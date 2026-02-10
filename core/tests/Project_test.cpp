#include <catch2/catch_test_macros.hpp>

#include "projection/core/Project.h"

using namespace projection::core;

TEST_CASE("Project stores metadata and cue order", "[project]") {
  ProjectSettings settings;
  settings.controllers["knob1"] = "hue";
  settings.midiChannels = {2};

  Project project{makeProjectId("proj-1"),
                  "Test",
                  "Demo project",
                  "2026-02-02T10:00:00Z",
                  "2026-02-02T10:10:00Z",
                  {makeAssetId("asset-1")},
                  {makeSceneId("scene-1")},
                  {makeFeedId("feed-1")},
                  {makeCueId("cue-1"), makeCueId("cue-2")},
                  settings};

  REQUIRE(project.getId().value == "proj-1");
  REQUIRE(project.getName() == "Test");
  REQUIRE(project.getCreatedAt() == "2026-02-02T10:00:00Z");
  REQUIRE(project.getUpdatedAt() == "2026-02-02T10:10:00Z");
  REQUIRE(project.getCueOrder().size() == 2);
  REQUIRE(project.getCueOrder()[1].value == "cue-2");
  REQUIRE(project.getSettings().controllers.at("knob1") == "hue");
}
