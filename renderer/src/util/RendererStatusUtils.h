#pragma once

#include <string>
#include <vector>

namespace projection::renderer {

struct DebugOverlayLine {
  std::string text;
  bool isError{false};
};

struct RendererStatusSnapshot {
  std::string host;
  int port{0};
  std::string role;
  std::string version;
  std::string sceneId;
  std::string lastCommand;
  std::string lastError;
};

inline std::string formatHelloCommand(const std::string& commandId) {
  return "Hello (#" + commandId + ")";
}

inline std::string formatLoadSceneCommand(const std::string& commandId, const std::string& projectId) {
  return "LoadScene (#" + commandId + ") project " + projectId;
}

inline std::string formatLoadSceneDefinitionCommand(const std::string& commandId) {
  return "LoadSceneDefinition (#" + commandId + ")";
}

inline std::string formatSetFeedForSurfaceCommand(const std::string& commandId,
                                                  const std::string& projectId,
                                                  const std::string& surfaceId,
                                                  const std::string& feedId) {
  return "SetFeedForSurface (#" + commandId + ") project " + projectId + " -> surface " + surfaceId + " feed " +
         feedId;
}

inline std::string formatPlayCueCommand(const std::string& commandId,
                                        const std::string& projectId,
                                        const std::string& cueId) {
  return "PlayCue (#" + commandId + ") project " + projectId + " -> cue " + cueId;
}

inline std::string formatShowTestPatternCommand(const std::string& commandId) {
  return "ShowTestPattern (#" + commandId + ")";
}

inline std::vector<DebugOverlayLine> buildDebugOverlayLines(const RendererStatusSnapshot& snapshot) {
  std::vector<DebugOverlayLine> lines;
  lines.push_back({"Renderer connected to: " + snapshot.host + ":" + std::to_string(snapshot.port), false});
  if (!snapshot.role.empty()) {
    lines.push_back({"Role: " + snapshot.role + " | Version: " + snapshot.version, false});
  }
  if (!snapshot.sceneId.empty()) {
    lines.push_back({"Loaded Scene: " + snapshot.sceneId, false});
  }
  if (!snapshot.lastCommand.empty()) {
    lines.push_back({"Last Command: " + snapshot.lastCommand, false});
  }
  if (!snapshot.lastError.empty()) {
    lines.push_back({"Last Error: " + snapshot.lastError, true});
  }
  return lines;
}

}  // namespace projection::renderer
