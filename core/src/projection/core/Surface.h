#pragma once

#include <string>
#include <vector>

#include "projection/core/Enums.h"
#include "projection/core/Feed.h"
#include "projection/core/Ids.h"

namespace projection::core {

struct Vec2 {
  float x{0.0f};
  float y{0.0f};

  bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
};

enum class SurfaceType { Polygon, Ellipse };

class Surface {
 public:
  Surface() = default;
  // Polygon surface constructor (legacy compatible)
  Surface(SurfaceId id, std::string name, std::vector<Vec2> vertices, FeedId feedId,
          float opacity = 1.0f, float brightness = 1.0f, BlendMode blendMode = BlendMode::Normal,
          int zOrder = 0);
  // Ellipse surface constructor
  Surface(SurfaceId id, std::string name, Vec2 center, float radiusX, float radiusY, FeedId feedId,
          float opacity = 1.0f, float brightness = 1.0f, BlendMode blendMode = BlendMode::Normal,
          int zOrder = 0);

  const SurfaceId& getId() const { return id_; }
  void setId(const SurfaceId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  SurfaceType getSurfaceType() const { return surfaceType_; }
  bool isEllipse() const { return surfaceType_ == SurfaceType::Ellipse; }
  bool isPolygon() const { return surfaceType_ == SurfaceType::Polygon; }

  // Polygon-specific
  const std::vector<Vec2>& getVertices() const { return vertices_; }
  void setVertices(const std::vector<Vec2>& vertices) { vertices_ = vertices; }

  // Ellipse-specific
  const Vec2& getCenter() const { return center_; }
  void setCenter(const Vec2& center) { center_ = center; }

  float getRadiusX() const { return radiusX_; }
  void setRadiusX(float rx) { radiusX_ = rx; }

  float getRadiusY() const { return radiusY_; }
  void setRadiusY(float ry) { radiusY_ = ry; }

  // Generate vertices for ellipse rendering (approximation as polygon)
  std::vector<Vec2> generateEllipseVertices(int numPoints = 32) const;

  const FeedId& getFeedId() const { return feedId_; }
  void setFeedId(const FeedId& feedId) { feedId_ = feedId; }

  float getOpacity() const { return opacity_; }
  void setOpacity(float opacity);

  float getBrightness() const { return brightness_; }
  void setBrightness(float brightness);

  BlendMode getBlendMode() const { return blendMode_; }
  void setBlendMode(BlendMode mode) { blendMode_ = mode; }

  int getZOrder() const { return zOrder_; }
  void setZOrder(int z) { zOrder_ = z; }

  float getRotation() const { return rotation_; }
  void setRotation(float degrees) { rotation_ = degrees; }

  bool isValid() const;

  bool operator==(const Surface& other) const {
    if (surfaceType_ != other.surfaceType_) return false;
    bool baseMatch = id_ == other.id_ && name_ == other.name_ && feedId_ == other.feedId_ &&
                     opacity_ == other.opacity_ && brightness_ == other.brightness_ &&
                     blendMode_ == other.blendMode_ && zOrder_ == other.zOrder_ && rotation_ == other.rotation_;
    if (!baseMatch) return false;
    if (surfaceType_ == SurfaceType::Ellipse) {
      return center_ == other.center_ && radiusX_ == other.radiusX_ && radiusY_ == other.radiusY_;
    }
    return vertices_ == other.vertices_;
  }

 private:
  static float clampUnit(float value);

  SurfaceId id_{};
  std::string name_{};
  SurfaceType surfaceType_{SurfaceType::Polygon};
  // Polygon data
  std::vector<Vec2> vertices_{};
  // Ellipse data
  Vec2 center_{0.0f, 0.0f};
  float radiusX_{0.45f};
  float radiusY_{0.45f};
  // Common properties
  FeedId feedId_{};
  float opacity_{1.0f};
  float brightness_{1.0f};
  BlendMode blendMode_{BlendMode::Normal};
  int zOrder_{0};
  float rotation_{0.0f};  // Video rotation in degrees
};

}  // namespace projection::core
