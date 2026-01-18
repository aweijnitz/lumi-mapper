import { createId } from "./useIds";
import type { Surface, Vec2 } from "../types/surface";

export type SurfaceShape = "rectangle" | "quad" | "circle";

type SurfaceFactoryOptions = {
  feedId: string;
  name?: string;
  zOrder?: number;
  index?: number;
};

// Generate circle vertices as a regular polygon
const generateCircleVertices = (numPoints: number = 32, radius: number = 0.5): Vec2[] => {
  const vertices: Vec2[] = [];
  for (let i = 0; i < numPoints; i++) {
    // Start from top (-PI/2) and go clockwise
    const angle = -Math.PI / 2 + (i / numPoints) * 2 * Math.PI;
    vertices.push({
      x: Math.cos(angle) * radius,
      y: Math.sin(angle) * radius,
    });
  }
  return vertices;
};

const shapeVertices: Record<SurfaceShape, Vec2[]> = {
  rectangle: [
    { x: -0.5, y: -0.5 },
    { x: 0.5, y: -0.5 },
    { x: 0.5, y: 0.5 },
    { x: -0.5, y: 0.5 },
  ],
  quad: [
    { x: -0.6, y: -0.5 },
    { x: 0.55, y: -0.55 },
    { x: 0.5, y: 0.55 },
    { x: -0.65, y: 0.45 },
  ],
  circle: generateCircleVertices(32, 0.45),
};

const shapeLabel = (shape: SurfaceShape) => {
  switch (shape) {
    case "quad":
      return "Quad";
    case "circle":
      return "Circle";
    case "rectangle":
    default:
      return "Rectangle";
  }
};

const resolveSurfaceName = (shape: SurfaceShape, options: SurfaceFactoryOptions) => {
  if (options.name && options.name.trim().length > 0) {
    return options.name.trim();
  }
  const suffix = options.index !== undefined ? ` ${options.index}` : "";
  return `${shapeLabel(shape)}${suffix}`;
};

export const createSurface = (shape: SurfaceShape, options: SurfaceFactoryOptions): Surface => {
  const vertices = shapeVertices[shape].map((vertex) => ({ ...vertex }));
  return {
    id: createId("surface"),
    name: resolveSurfaceName(shape, options),
    vertices,
    feedId: options.feedId,
    opacity: 1,
    brightness: 1,
    blendMode: "Normal",
    zOrder: options.zOrder ?? 0,
    rotation: 0,
  };
};
