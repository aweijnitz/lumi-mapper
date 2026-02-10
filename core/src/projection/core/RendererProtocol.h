#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "projection/core/Asset.h"
#include "projection/core/Ids.h"
#include "projection/core/Scene.h"
#include "projection/core/Feed.h"

namespace projection::core {

// Renderer ↔ Server protocol messages shared between components.
//
// Error handling: from_json overloads throw std::runtime_error when JSON is missing
// required fields, uses the wrong type, or contains invalid enum strings.

enum class RendererMessageType {
  Hello,
  Ack,
  Error,
  LoadScene,
  LoadSceneDefinition,
  SetFeedForSurface,
  PlayCue,
  ShowTestPattern,
  ShowCrosshair
};

struct RendererMessageBase {
  RendererMessageType type;
  std::string commandId;

  bool operator==(const RendererMessageBase& other) const {
    return type == other.type && commandId == other.commandId;
  }
};

struct HelloMessage {
  std::string version;
  std::string role;
  std::string name;
  int width{0};
  int height{0};

  bool operator==(const HelloMessage& other) const {
    return version == other.version && role == other.role && name == other.name && width == other.width &&
           height == other.height;
  }
};

struct AckMessage {
  std::string commandId;

  bool operator==(const AckMessage& other) const { return commandId == other.commandId; }
};

struct ErrorMessage {
  std::string commandId;
  std::string message;

  bool operator==(const ErrorMessage& other) const {
    return commandId == other.commandId && message == other.message;
  }
};

struct LoadSceneMessage {
  ProjectId projectId;
  SceneId sceneId;

  bool operator==(const LoadSceneMessage& other) const {
    return projectId == other.projectId && sceneId == other.sceneId;
  }
};

struct LoadSceneDefinitionMessage {
  Scene scene;
  std::vector<Feed> feeds;
  std::vector<Asset> assets;

  bool operator==(const LoadSceneDefinitionMessage& other) const {
    return scene == other.scene && feeds == other.feeds && assets == other.assets;
  }
};

struct SetFeedForSurfaceMessage {
  ProjectId projectId;
  SurfaceId surfaceId;
  FeedId feedId;

  bool operator==(const SetFeedForSurfaceMessage& other) const {
    return projectId == other.projectId && surfaceId == other.surfaceId && feedId == other.feedId;
  }
};

struct PlayCueMessage {
  ProjectId projectId;
  CueId cueId;

  bool operator==(const PlayCueMessage& other) const { return projectId == other.projectId && cueId == other.cueId; }
};

struct ShowTestPatternMessage {
  bool enabled;

  bool operator==(const ShowTestPatternMessage& other) const { return enabled == other.enabled; }
};

struct ShowCrosshairMessage {
  bool enabled;      // false to hide crosshair
  float x;           // normalized x coordinate (-1 to 1)
  float y;           // normalized y coordinate (-1 to 1)

  bool operator==(const ShowCrosshairMessage& other) const {
    return enabled == other.enabled && x == other.x && y == other.y;
  }
};

struct RendererMessage {
  RendererMessageType type;
  std::string commandId;
  std::optional<HelloMessage> hello;
  std::optional<AckMessage> ack;
  std::optional<ErrorMessage> error;
  std::optional<LoadSceneMessage> loadScene;
  std::optional<LoadSceneDefinitionMessage> loadSceneDefinition;
  std::optional<SetFeedForSurfaceMessage> setFeedForSurface;
  std::optional<PlayCueMessage> playCue;
  std::optional<ShowTestPatternMessage> showTestPattern;
  std::optional<ShowCrosshairMessage> showCrosshair;

  bool operator==(const RendererMessage& other) const {
    return type == other.type && commandId == other.commandId && hello == other.hello &&
           ack == other.ack && error == other.error && loadScene == other.loadScene &&
           loadSceneDefinition == other.loadSceneDefinition &&
           setFeedForSurface == other.setFeedForSurface && playCue == other.playCue &&
           showTestPattern == other.showTestPattern && showCrosshair == other.showCrosshair;
  }
};

// JSON serialization helpers.
void to_json(nlohmann::json& j, const RendererMessageType& type);
void from_json(const nlohmann::json& j, RendererMessageType& type);

void to_json(nlohmann::json& j, const HelloMessage& message);
void from_json(const nlohmann::json& j, HelloMessage& message);

void to_json(nlohmann::json& j, const AckMessage& message);
void from_json(const nlohmann::json& j, AckMessage& message);

void to_json(nlohmann::json& j, const ErrorMessage& message);
void from_json(const nlohmann::json& j, ErrorMessage& message);

void to_json(nlohmann::json& j, const LoadSceneMessage& message);
void from_json(const nlohmann::json& j, LoadSceneMessage& message);

void to_json(nlohmann::json& j, const LoadSceneDefinitionMessage& message);
void from_json(const nlohmann::json& j, LoadSceneDefinitionMessage& message);

void to_json(nlohmann::json& j, const SetFeedForSurfaceMessage& message);
void from_json(const nlohmann::json& j, SetFeedForSurfaceMessage& message);

void to_json(nlohmann::json& j, const PlayCueMessage& message);
void from_json(const nlohmann::json& j, PlayCueMessage& message);

void to_json(nlohmann::json& j, const ShowTestPatternMessage& message);
void from_json(const nlohmann::json& j, ShowTestPatternMessage& message);

void to_json(nlohmann::json& j, const ShowCrosshairMessage& message);
void from_json(const nlohmann::json& j, ShowCrosshairMessage& message);

void to_json(nlohmann::json& j, const RendererMessage& message);
void from_json(const nlohmann::json& j, RendererMessage& message);

}  // namespace projection::core
