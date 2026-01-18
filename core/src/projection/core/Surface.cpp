#include "projection/core/Surface.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace projection::core {

// Polygon surface constructor
Surface::Surface(SurfaceId id, std::string name, std::vector<Vec2> vertices, FeedId feedId,
                 float opacity, float brightness, BlendMode blendMode, int zOrder)
    : id_(std::move(id)),
      name_(std::move(name)),
      surfaceType_(SurfaceType::Polygon),
      vertices_(std::move(vertices)),
      feedId_(std::move(feedId)),
      opacity_(clampUnit(opacity)),
      brightness_(clampUnit(brightness)),
      blendMode_(blendMode),
      zOrder_(zOrder) {}

// Ellipse surface constructor
Surface::Surface(SurfaceId id, std::string name, Vec2 center, float radiusX, float radiusY, FeedId feedId,
                 float opacity, float brightness, BlendMode blendMode, int zOrder)
    : id_(std::move(id)),
      name_(std::move(name)),
      surfaceType_(SurfaceType::Ellipse),
      center_(center),
      radiusX_(radiusX),
      radiusY_(radiusY),
      feedId_(std::move(feedId)),
      opacity_(clampUnit(opacity)),
      brightness_(clampUnit(brightness)),
      blendMode_(blendMode),
      zOrder_(zOrder) {}

void Surface::setOpacity(float opacity) { opacity_ = clampUnit(opacity); }

void Surface::setBrightness(float brightness) { brightness_ = clampUnit(brightness); }

std::vector<Vec2> Surface::generateEllipseVertices(int numPoints) const {
  std::vector<Vec2> vertices;
  vertices.reserve(numPoints);
  constexpr float PI = 3.14159265358979323846f;
  for (int i = 0; i < numPoints; i++) {
    // Start from top (-PI/2) and go clockwise
    float angle = -PI / 2.0f + (static_cast<float>(i) / static_cast<float>(numPoints)) * 2.0f * PI;
    vertices.push_back({
        center_.x + std::cos(angle) * radiusX_,
        center_.y + std::sin(angle) * radiusY_
    });
  }
  return vertices;
}

bool Surface::isValid() const {
  if (surfaceType_ == SurfaceType::Ellipse) {
    if (radiusX_ <= 0.0f || radiusY_ <= 0.0f) {
      return false;
    }
  } else {
    if (vertices_.size() < 3) {
      return false;
    }
  }
  if (opacity_ < 0.0f || opacity_ > 1.0f) {
    return false;
  }
  if (brightness_ < 0.0f || brightness_ > 1.0f) {
    return false;
  }
  return true;
}

float Surface::clampUnit(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

}  // namespace projection::core
