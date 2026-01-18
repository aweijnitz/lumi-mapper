export type Vec2 = {
  x: number;
  y: number;
};

export type BlendMode = "Normal" | "Additive" | "Multiply";

export type Surface = {
  id: string;
  name: string;
  vertices: Vec2[];
  feedId: string;
  opacity: number;
  brightness: number;
  blendMode: BlendMode;
  zOrder: number;
  rotation: number;  // Video rotation in degrees
};
