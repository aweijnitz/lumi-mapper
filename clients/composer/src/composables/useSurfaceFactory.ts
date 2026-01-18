import { createId } from "./useIds";
import type { Surface, Vec2, PolygonSurface, EllipseSurface } from "../types/surface";

export type SurfaceShape = "rectangle" | "quad" | "ellipse";

type SurfaceFactoryOptions = {
  feedId: string;
  name?: string;
  zOrder?: number;
  index?: number;
};

const shapeVertices: Record<Exclude<SurfaceShape, "ellipse">, Vec2[]> = {
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
};

const shapeLabel = (shape: SurfaceShape) => {
  switch (shape) {
    case "quad":
      return "Quad";
    case "ellipse":
      return "Ellipse";
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
  if (shape === "ellipse") {
    return createEllipseSurface(options);
  }
  return createPolygonSurface(shape, options);
};

const createPolygonSurface = (shape: Exclude<SurfaceShape, "ellipse">, options: SurfaceFactoryOptions): PolygonSurface => {
  const vertices = shapeVertices[shape].map((vertex) => ({ ...vertex }));
  return {
    id: createId("surface"),
    name: resolveSurfaceName(shape, options),
    surfaceType: "polygon",
    vertices,
    feedId: options.feedId,
    opacity: 1,
    brightness: 1,
    blendMode: "Normal",
    zOrder: options.zOrder ?? 0,
    rotation: 0,
  };
};

const createEllipseSurface = (options: SurfaceFactoryOptions): EllipseSurface => {
  return {
    id: createId("surface"),
    name: resolveSurfaceName("ellipse", options),
    surfaceType: "ellipse",
    center: { x: 0, y: 0 },
    radiusX: 0.45,
    radiusY: 0.45,
    feedId: options.feedId,
    opacity: 1,
    brightness: 1,
    blendMode: "Normal",
    zOrder: options.zOrder ?? 0,
    rotation: 0,
  };
};
