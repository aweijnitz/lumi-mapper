#pragma once

#include <string>

namespace projection::core {

enum class AssetType { VideoFile, ImageFile };

// Pan direction for image feeds
enum class PanDirection { LeftToRight, RightToLeft, PingPong };

enum class BlendMode { Normal, Additive, Multiply };

// Convert AssetType to a readable string representation.
inline std::string toString(AssetType type) {
  switch (type) {
    case AssetType::VideoFile:
      return "VideoFile";
    case AssetType::ImageFile:
      return "ImageFile";
  }
  return "Unknown";
}

// Parse an AssetType from a string. Returns true on success.
inline bool fromString(const std::string& value, AssetType& outType) {
  if (value == "VideoFile") {
    outType = AssetType::VideoFile;
    return true;
  }
  if (value == "ImageFile") {
    outType = AssetType::ImageFile;
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
