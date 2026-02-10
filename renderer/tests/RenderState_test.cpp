#include <catch2/catch_test_macros.hpp>

#include "RenderState.h"

#include <vector>

#include <projection/core/Asset.h>
#include <projection/core/Feed.h>
#include <projection/core/Scene.h>
#include <projection/core/Surface.h>

using projection::core::Asset;
using projection::core::AssetId;
using projection::core::AssetType;
using projection::core::Feed;
using projection::core::FeedId;
using projection::core::ProjectId;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using projection::renderer::RenderState;
using projection::renderer::mapVideoFeedFilePaths;

TEST_CASE("mapVideoFeedFilePaths returns mappings for video feeds", "[renderer][renderstate]") {
  Scene scene{ProjectId{"project-1"}, SceneId{"scene-1"}, "Test Scene", "desc", {}};
  std::vector<Asset> assets{
      Asset(AssetId{"asset-video"}, "Video", AssetType::VideoFile, "/media/video1.mp4"),
      Asset(AssetId{"asset-image"}, "Image", AssetType::ImageFile, "/media/image1.png")};
  std::vector<Feed> feeds{
      Feed{ProjectId{"project-1"}, FeedId{"video1"}, "Video 1", AssetId{"asset-video"}},
      Feed{ProjectId{"project-1"}, FeedId{"image1"}, "Image 1", AssetId{"asset-image"}}};

  auto mapping = mapVideoFeedFilePaths(scene, feeds, assets);

  REQUIRE(mapping.size() == 1);
  REQUIRE(mapping.at("video1") == "/media/video1.mp4");
  REQUIRE(mapping.find("image1") == mapping.end());
}

TEST_CASE("mapVideoFeedFilePaths skips feeds with missing assets", "[renderer][renderstate]") {
  Scene scene{ProjectId{"project-1"}, SceneId{"scene-2"}, "Scene", "desc", {}};
  std::vector<Feed> feeds{Feed{ProjectId{"project-1"}, FeedId{"video2"}, "Video 2", AssetId{"missing"}}};

  auto mapping = mapVideoFeedFilePaths(scene, feeds, {});
  REQUIRE(mapping.empty());
}

TEST_CASE("loadSceneDefinition stores scene, feeds, assets, and video resources", "[renderer][renderstate]") {
  Surface surface{SurfaceId{"surface-1"}, "Surface", {Vec2{0, 0}, Vec2{100, 0}, Vec2{100, 100}, Vec2{0, 100}},
                  FeedId{"video1"}};
  Scene scene{ProjectId{"project-1"}, SceneId{"scene-3"}, "Scene 3", "desc", {surface}};
  std::vector<Asset> assets{Asset(AssetId{"asset-video"}, "Video", AssetType::VideoFile, "/media/video1.mp4")};
  std::vector<Feed> feeds{Feed{ProjectId{"project-1"}, FeedId{"video1"}, "Video 1", AssetId{"asset-video"}}};

  RenderState state;
  state.loadSceneDefinition(scene, feeds, assets);

  REQUIRE(state.currentScene().getId().value == scene.getId().value);
  REQUIRE(state.currentFeeds().size() == feeds.size());
  REQUIRE(state.currentAssets().size() == assets.size());

  const auto& videoFeeds = state.videoFeeds();
  REQUIRE(videoFeeds.size() == 1);
  auto it = videoFeeds.find("video1");
  REQUIRE(it != videoFeeds.end());
  REQUIRE(it->second.filePath == "/media/video1.mp4");
}

