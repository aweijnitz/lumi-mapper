import type { Surface } from "./surface";

// Available scene-level filter types
export type SceneFilter = "none" | "colorTint" | "monochrome";

// Scene-level settings for visual effects and rendering options
export type SceneSettings = {
  filter: SceneFilter;           // Active filter for the scene
  colorPaletteIndex: number;     // Which color palette to use for colorTint filter (0-based)
};

// Default settings for new scenes
export const defaultSceneSettings: SceneSettings = {
  filter: "none",
  colorPaletteIndex: 0,
};

// Human-readable labels for filters
export const sceneFilterLabels: Record<SceneFilter, string> = {
  none: "None",
  colorTint: "Color Tint",
  monochrome: "Monochrome",
};

export type Scene = {
  projectId: string;
  id: string;
  name: string;
  description: string;
  surfaces: Surface[];
  settings?: SceneSettings;  // Optional for backwards compatibility
};
