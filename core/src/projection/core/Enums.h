#pragma once

#include <string>

namespace projection::core {

enum class FeedType { VideoFile, ImageFile, Camera, Generated };

// Pan direction for image feeds
enum class PanDirection { LeftToRight, RightToLeft, PingPong };

enum class BlendMode { Normal, Additive, Multiply };

// Convert FeedType to a readable string representation.
inline std::string toString(FeedType type) {
  switch (type) {
    case FeedType::VideoFile:
      return "VideoFile";
    case FeedType::ImageFile:
      return "ImageFile";
    case FeedType::Camera:
      return "Camera";
    case FeedType::Generated:
      return "Generated";
  }
  return "Unknown";
}

// Parse a FeedType from a string. Returns true on success.
inline bool fromString(const std::string& value, FeedType& outType) {
  if (value == "VideoFile") {
    outType = FeedType::VideoFile;
    return true;
  }
  if (value == "ImageFile") {
    outType = FeedType::ImageFile;
    return true;
  }
  if (value == "Camera") {
    outType = FeedType::Camera;
    return true;
  }
  if (value == "Generated") {
    outType = FeedType::Generated;
    return true;
  }
  return false;
}

// Convert PanDirection to a readable string representation.
inline std::string toString(PanDirection direction) {
  switch (direction) {
    case PanDirection::LeftToRight:
      return "leftToRight";
    case PanDirection::RightToLeft:
      return "rightToLeft";
    case PanDirection::PingPong:
      return "pingPong";
  }
  return "leftToRight";
}

// Parse a PanDirection from a string. Returns true on success.
inline bool fromString(const std::string& value, PanDirection& outDirection) {
  if (value == "leftToRight") {
    outDirection = PanDirection::LeftToRight;
    return true;
  }
  if (value == "rightToLeft") {
    outDirection = PanDirection::RightToLeft;
    return true;
  }
  if (value == "pingPong") {
    outDirection = PanDirection::PingPong;
    return true;
  }
  return false;
}

// Convert BlendMode to a readable string representation.
inline std::string toString(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return "Normal";
    case BlendMode::Additive:
      return "Additive";
    case BlendMode::Multiply:
      return "Multiply";
  }
  return "Unknown";
}

// Parse a BlendMode from a string. Returns true on success.
inline bool fromString(const std::string& value, BlendMode& outMode) {
  if (value == "Normal") {
    outMode = BlendMode::Normal;
    return true;
  }
  if (value == "Additive") {
    outMode = BlendMode::Additive;
    return true;
  }
  if (value == "Multiply") {
    outMode = BlendMode::Multiply;
    return true;
  }
  return false;
}

}  // namespace projection::core
