#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "projection/core/RendererProtocol.h"
#include "projection/core/Serialization.h"

using projection::core::Asset;
using projection::core::AssetId;
using projection::core::AssetType;
using projection::core::Feed;
using projection::core::FeedId;
using projection::core::LoadSceneDefinitionMessage;
using projection::core::ProjectId;
using projection::core::RendererMessage;
using projection::core::RendererMessageType;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using nlohmann::json;

Scene makeScene() {
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{SurfaceId{"surface-a"}, "Left", quad, FeedId{"feed-a"}};
  Surface surfaceB{SurfaceId{"surface-b"}, "Right", quad, FeedId{"feed-b"}};
  return Scene(ProjectId{"project-1"}, SceneId{"scene-1"}, "Example Scene", "With two surfaces",
               {surfaceA, surfaceB});
}

std::vector<Feed> makeFeeds() {
  return {Feed(ProjectId{"project-1"}, FeedId{"feed-a"}, "Feed A", AssetId{"asset-a"}),
          Feed(ProjectId{"project-1"}, FeedId{"feed-b"}, "Feed B", AssetId{"asset-b"})};
}

std::vector<Asset> makeAssets() {
  return {Asset(AssetId{"asset-a"}, "Clip A", AssetType::VideoFile, "a.mp4"),
          Asset(AssetId{"asset-b"}, "Clip B", AssetType::VideoFile, "b.mp4")};
}

TEST_CASE("RendererProtocol round trip LoadSceneDefinition", "[RendererProtocol]") {
  RendererMessage message{};
  message.type = RendererMessageType::LoadSceneDefinition;
  message.commandId = "cmd-load-def";
  message.loadSceneDefinition = LoadSceneDefinitionMessage{makeScene(), makeFeeds(), makeAssets()};

  json serialized = message;
  auto parsed = serialized.get<RendererMessage>();

  REQUIRE(parsed.type == RendererMessageType::LoadSceneDefinition);
  REQUIRE(parsed.loadSceneDefinition.has_value());
  REQUIRE(parsed.loadSceneDefinition->scene == message.loadSceneDefinition->scene);
  REQUIRE(parsed.loadSceneDefinition->feeds == message.loadSceneDefinition->feeds);
  REQUIRE(parsed.loadSceneDefinition->assets == message.loadSceneDefinition->assets);
}

TEST_CASE("RendererProtocol LoadSceneDefinition requires scene", "[RendererProtocol]") {
  json missingScene = {{"type", "loadSceneDefinition"},
                       {"commandId", "cmd"},
                       {"payload", json{{"feeds", json::array()}, {"assets", json::array()}}}};

  bool threw = false;
  try {
    missingScene.get<RendererMessage>();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("RendererProtocol LoadSceneDefinition requires feeds", "[RendererProtocol]") {
  json missingFeeds = {{"type", "loadSceneDefinition"},
                       {"commandId", "cmd"},
                       {"payload", json{{"scene", json::object()}, {"assets", json::array()}}}};

  bool threw = false;
  try {
    missingFeeds.get<RendererMessage>();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("RendererProtocol LoadSceneDefinition requires assets", "[RendererProtocol]") {
  json missingAssets = {{"type", "loadSceneDefinition"},
                        {"commandId", "cmd"},
                        {"payload", json{{"scene", json::object()}, {"feeds", json::array()}}}};

  bool threw = false;
  try {
    missingAssets.get<RendererMessage>();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("RendererProtocol LoadSceneDefinition validates feed payloads", "[RendererProtocol]") {
  json invalidFeed = {{"type", "loadSceneDefinition"},
                      {"commandId", "cmd"},
                      {"payload",
                       json{{"scene", json::object()},
                            {"feeds", json::array({json{{"id", "missing-fields"}}})},
                            {"assets", json::array()}}}};

  bool threw = false;
  try {
    invalidFeed.get<RendererMessage>();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}
