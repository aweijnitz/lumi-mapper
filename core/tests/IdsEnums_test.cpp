#include <catch2/catch_test_macros.hpp>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

using namespace projection::core;

TEST_CASE("Identifier wrappers compare by value", "[ids]") {
  SceneId sceneA{"scene-1"};
  SceneId sceneB{"scene-1"};
  SceneId sceneC{"scene-2"};

  REQUIRE(sceneA == sceneB);
  REQUIRE(sceneA != sceneC);

  SurfaceId surface = makeSurfaceId("surface-1");
  REQUIRE(surface == SurfaceId{"surface-1"});

  FeedId feedOne{"feed-1"};
  FeedId feedTwo{"feed-2"};
  REQUIRE(feedOne != feedTwo);

  AssetId asset = makeAssetId("asset-1");
  REQUIRE(asset == AssetId{"asset-1"});

  CueId cue = makeCueId("cue-1");
  REQUIRE(cue.value == std::string("cue-1"));

  ProjectId project = makeProjectId("proj-1");
  REQUIRE(project == ProjectId{"proj-1"});
}

TEST_CASE("AssetType string conversion succeeds for known values", "[enums]") {
  REQUIRE(toString(AssetType::VideoFile) == "VideoFile");
  REQUIRE(toString(AssetType::ImageFile) == "ImageFile");

  AssetType type{};
  REQUIRE(fromString("VideoFile", type));
  REQUIRE(type == AssetType::VideoFile);
  REQUIRE(fromString("ImageFile", type));
  REQUIRE(type == AssetType::ImageFile);
}

TEST_CASE("BlendMode string conversion succeeds for known values", "[enums]") {
  REQUIRE(toString(BlendMode::Normal) == "Normal");
  REQUIRE(toString(BlendMode::Additive) == "Additive");
  REQUIRE(toString(BlendMode::Multiply) == "Multiply");

  BlendMode mode{};
  REQUIRE(fromString("Normal", mode));
  REQUIRE(mode == BlendMode::Normal);
  REQUIRE(fromString("Additive", mode));
  REQUIRE(mode == BlendMode::Additive);
  REQUIRE(fromString("Multiply", mode));
  REQUIRE(mode == BlendMode::Multiply);
}

TEST_CASE("Enum parsing fails gracefully for invalid strings", "[enums]") {
  AssetType assetType = AssetType::VideoFile;
  BlendMode blendMode = BlendMode::Normal;

  REQUIRE(!fromString("", assetType));
  REQUIRE(!fromString("unknown", assetType));
  REQUIRE(assetType == AssetType::VideoFile);

  REQUIRE(!fromString("Invalid", blendMode));
  REQUIRE(!fromString("123", blendMode));
  REQUIRE(blendMode == BlendMode::Normal);
}
