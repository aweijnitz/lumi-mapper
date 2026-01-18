export type Vec2 = {
  x: number;
  y: number;
};

export type BlendMode = "Normal" | "Additive" | "Multiply";

export type SurfaceType = "polygon" | "ellipse";

// Polygon surface - defined by vertices
export type PolygonSurface = {
  id: string;
  name: string;
  surfaceType: "polygon";
  vertices: Vec2[];
  feedId: string;
  opacity: number;
  brightness: number;
  blendMode: BlendMode;
  zOrder: number;
  rotation: number;  // Video rotation in degrees
};

// Ellipse surface - defined by center and radii
export type EllipseSurface = {
  id: string;
  name: string;
  surfaceType: "ellipse";
  center: Vec2;
  radiusX: number;  // Horizontal radius in normalized coords (-1 to 1 scale)
  radiusY: number;  // Vertical radius in normalized coords (-1 to 1 scale)
  feedId: string;
  opacity: number;
  brightness: number;
  blendMode: BlendMode;
  zOrder: number;
  rotation: number;  // Video rotation in degrees
};

// Legacy polygon surface (for backwards compatibility during migration)
export type LegacyPolygonSurface = {
  id: string;
  name: string;
  vertices: Vec2[];
  feedId: string;
  opacity: number;
  brightness: number;
  blendMode: BlendMode;
  zOrder: number;
  rotation: number;
};

export type Surface = PolygonSurface | EllipseSurface | LegacyPolygonSurface;

// Type guards
export const isEllipseSurface = (surface: Surface): surface is EllipseSurface =>
  "surfaceType" in surface && surface.surfaceType === "ellipse";

export const isPolygonSurface = (surface: Surface): surface is PolygonSurface =>
  ("surfaceType" in surface && surface.surfaceType === "polygon") ||
  (!("surfaceType" in surface) && "vertices" in surface);

// Helper to get vertices for any surface type (ellipse generates approximate vertices)
export const getSurfaceVertices = (surface: Surface, numPoints = 32): Vec2[] => {
  if (isEllipseSurface(surface)) {
    const vertices: Vec2[] = [];
    for (let i = 0; i < numPoints; i++) {
      const angle = -Math.PI / 2 + (i / numPoints) * 2 * Math.PI;
      vertices.push({
        x: surface.center.x + Math.cos(angle) * surface.radiusX,
        y: surface.center.y + Math.sin(angle) * surface.radiusY,
      });
    }
    return vertices;
  }
  return surface.vertices;
};
