#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <nlohmann/json.hpp>

#include "projection/core/Asset.h"
#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Serialization.h"
#include "projection/core/Surface.h"

using projection::core::Asset;
using projection::core::AssetId;
using projection::core::AssetType;
using projection::core::AssetVariant;
using projection::core::BlendMode;
using projection::core::Cue;
using projection::core::CueId;
using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedSettings;
using projection::core::Project;
using projection::core::ProjectId;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using nlohmann::json;

namespace {

void expectRuntimeError(const std::function<void()>& fn) {
  bool threw = false;
  try {
    fn();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}

}  // namespace

TEST_CASE("Asset round-trip serialization", "[serialization]") {
  Asset asset{AssetId{"asset-1"}, "Clip A", AssetType::VideoFile, "data/assets/clipA.mp4",
              {AssetVariant{"data/assets/clipA_low.mp4", "lowres"}}};

  json j = asset;
  Asset parsed = j.get<Asset>();

  REQUIRE(parsed.getId().value == asset.getId().value);
  REQUIRE(parsed.getName() == asset.getName());
  REQUIRE(parsed.getType() == asset.getType());
  REQUIRE(parsed.getPath() == asset.getPath());
  REQUIRE(parsed.getVariants() == asset.getVariants());
}

TEST_CASE("Feed round-trip serialization", "[serialization]") {
  FeedSettings settings;
  settings.variantPath = "data/assets/clipA_low.mp4";
  settings.monochrome = true;

  Feed feed{ProjectId{"project-1"}, FeedId{"feed-1"}, "Feed A", AssetId{"asset-1"}, settings};

  json j = feed;
  Feed parsed = j.get<Feed>();

  REQUIRE(parsed.getId().value == feed.getId().value);
  REQUIRE(parsed.getName() == feed.getName());
  REQUIRE(parsed.getAssetId().value == feed.getAssetId().value);
  REQUIRE(parsed.getSettings() == feed.getSettings());
}

TEST_CASE("Surface round-trip serialization", "[serialization]") {
  std::vector<Vec2> verts{{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
  Surface surface{SurfaceId{"surface-1"}, "Quad", verts, FeedId{"feed-1"}, 0.8f, 0.9f,
                  BlendMode::Multiply, 2};

  json j = surface;
  Surface parsed = j.get<Surface>();

  REQUIRE(parsed.getId().value == surface.getId().value);
  REQUIRE(parsed.getName() == surface.getName());
  REQUIRE(parsed.getFeedId().value == surface.getFeedId().value);
  REQUIRE(std::abs(parsed.getOpacity() - surface.getOpacity()) < 1e-5f);
  REQUIRE(std::abs(parsed.getBrightness() - surface.getBrightness()) < 1e-5f);
  REQUIRE(parsed.getBlendMode() == surface.getBlendMode());
  REQUIRE(parsed.getZOrder() == surface.getZOrder());
  REQUIRE(parsed.getVertices().size() == surface.getVertices().size());
  for (size_t i = 0; i < parsed.getVertices().size(); ++i) {
    REQUIRE(std::abs(parsed.getVertices()[i].x - surface.getVertices()[i].x) < 1e-5f);
    REQUIRE(std::abs(parsed.getVertices()[i].y - surface.getVertices()[i].y) < 1e-5f);
  }
}

TEST_CASE("Scene round-trip serialization", "[serialization]") {
  std::vector<Vec2> verts{{0.f, 0.f}, {2.f, 0.f}, {2.f, 2.f}, {0.f, 2.f}};
  Surface surfaceA{SurfaceId{"sA"}, "Left", verts, FeedId{"feedA"}, 1.0f, 1.0f, BlendMode::Normal, 0};
  Surface surfaceB{SurfaceId{"sB"}, "Right", verts, FeedId{"feedB"}, 0.7f, 0.8f, BlendMode::Additive, 1};
  Scene scene{ProjectId{"project-1"}, SceneId{"scene-123"}, "Main Scene", "Two quads", {surfaceA, surfaceB}};

  json j = scene;
  Scene parsed = j.get<Scene>();

  REQUIRE(parsed.getId().value == scene.getId().value);
  REQUIRE(parsed.getName() == scene.getName());
  REQUIRE(parsed.getDescription() == scene.getDescription());
  REQUIRE(parsed.getSurfaces().size() == scene.getSurfaces().size());
}

TEST_CASE("Cue round-trip serialization", "[serialization]") {
  Cue cue{ProjectId{"project-1"}, CueId{"cue-1"}, "Intro", SceneId{"scene-123"}};
  cue.getSurfaceOpacities()[SurfaceId{"sA"}] = 0.5f;
  cue.getSurfaceBrightnesses()[SurfaceId{"sA"}] = 0.7f;
  cue.getSurfaceOpacities()[SurfaceId{"sB"}] = 1.0f;
  cue.getSurfaceBrightnesses()[SurfaceId{"sB"}] = 1.2f;

  json j = cue;
  Cue parsed = j.get<Cue>();

  REQUIRE(parsed.getId().value == cue.getId().value);
  REQUIRE(parsed.getName() == cue.getName());
  REQUIRE(parsed.getSceneId().value == cue.getSceneId().value);
  REQUIRE(parsed.getSurfaceOpacities().size() == cue.getSurfaceOpacities().size());
  REQUIRE(parsed.getSurfaceBrightnesses().size() == cue.getSurfaceBrightnesses().size());
  for (const auto& [surfaceId, opacity] : cue.getSurfaceOpacities()) {
    REQUIRE(std::abs(parsed.getSurfaceOpacities().at(surfaceId) - opacity) < 1e-5f);
  }
  for (const auto& [surfaceId, brightness] : cue.getSurfaceBrightnesses()) {
    REQUIRE(std::abs(parsed.getSurfaceBrightnesses().at(surfaceId) - brightness) < 1e-5f);
  }
}

TEST_CASE("Project round-trip serialization", "[serialization]") {
  ProjectSettings settings;
  settings.controllers["fader1"] = "masterBrightness";
  settings.midiChannels = {1, 10};
  settings.globalConfig["clockBpm"] = "128";

  Project project{ProjectId{"proj-1"},
                  "Main Show",
                  "Demo project",
                  "2026-02-02T10:00:00Z",
                  "2026-02-02T10:10:00Z",
                  {AssetId{"asset-1"}, AssetId{"asset-2"}},
                  {SceneId{"scene-1"}},
                  {FeedId{"feed-1"}},
                  {CueId{"cue-1"}, CueId{"cue-2"}},
                  settings};

  json j = project;
  Project parsed = j.get<Project>();

  REQUIRE(parsed.getId().value == project.getId().value);
  REQUIRE(parsed.getName() == project.getName());
  REQUIRE(parsed.getDescription() == project.getDescription());
  REQUIRE(parsed.getCreatedAt() == project.getCreatedAt());
  REQUIRE(parsed.getUpdatedAt() == project.getUpdatedAt());
  REQUIRE(parsed.getAssetIds().size() == project.getAssetIds().size());
  REQUIRE(parsed.getSceneIds().size() == project.getSceneIds().size());
  REQUIRE(parsed.getFeedIds().size() == project.getFeedIds().size());
  REQUIRE(parsed.getCueOrder().size() == project.getCueOrder().size());
  REQUIRE(parsed.getSettings().controllers.at("fader1") == "masterBrightness");
  std::vector<int> expectedChannels{1, 10};
  REQUIRE(parsed.getSettings().midiChannels == expectedChannels);
  REQUIRE(parsed.getSettings().globalConfig.at("clockBpm") == "128");
}

TEST_CASE("Invalid enum strings throw", "[serialization][negative]") {
  json invalidAsset = {{"id", "asset-1"},
                       {"name", "Invalid"},
                       {"type", "NotAType"},
                       {"path", "data/assets/clipA.mp4"}};
  expectRuntimeError([&]() { invalidAsset.get<Asset>(); });

  json invalidSurface = {"id", "s1",
                         {"name", "Surf"},
                         {"vertices", json::array({{{"x", 0}, {"y", 0}}})},
                         {"feedId", "feed"},
                         {"opacity", 1.0},
                         {"brightness", 1.0},
                         {"blendMode", "BadMode"},
                         {"zOrder", 0}};
  expectRuntimeError([&]() { invalidSurface.get<Surface>(); });
}

TEST_CASE("Missing required fields throw", "[serialization][negative]") {
  json missingId = {{"projectId", "project-1"}, {"name", "No Id"}, {"assetId", "asset-1"}};
  expectRuntimeError([&]() { missingId.get<Feed>(); });

  json missingProjectFields = {{"id", "proj-1"},
                               {"name", "Bad"},
                               {"description", "desc"},
                               {"createdAt", "2026-02-02T10:00:00Z"},
                               {"updatedAt", "2026-02-02T10:10:00Z"},
                               {"assetIds", json::array()},
                               {"sceneIds", json::array()},
                               {"feedIds", json::array()}};
  expectRuntimeError([&]() { missingProjectFields.get<Project>(); });
}

TEST_CASE("Type mismatches throw", "[serialization][negative]") {
  json wrongType = {{"projectId", "project-1"},
                    {"id", 123},
                    {"name", "Bad"},
                    {"assetId", "asset-1"}};
  expectRuntimeError([&]() { wrongType.get<Feed>(); });

  json badVertices = {{"id", "s1"},
                      {"name", "Surf"},
                      {"vertices", json::array({{"x", 0}, {"y", 0}})},
                      {"feedId", "feed"},
                      {"opacity", "one"},
                      {"brightness", 1.0},
                      {"blendMode", "Normal"},
                      {"zOrder", 0}};
  expectRuntimeError([&]() { badVertices.get<Surface>(); });

  json badProject = {{"id", "proj-1"},
                     {"name", "Bad"},
                     {"description", "desc"},
                     {"createdAt", "2026-02-02T10:00:00Z"},
                     {"updatedAt", "2026-02-02T10:10:00Z"},
                     {"assetIds", json::array({{"not-a-string"}})},
                     {"sceneIds", json::array()},
                     {"feedIds", json::array()},
                     {"cueOrder", json::array()},
                     {"settings", json{{"midiChannels", json::array({1, 2})}}}};
  expectRuntimeError([&]() { badProject.get<Project>(); });
}
