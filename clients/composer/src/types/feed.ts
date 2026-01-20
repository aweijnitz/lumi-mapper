export type FeedType = "VideoFile" | "ImageFile";

export type VideoFileConfig = {
  filePath: string;
};

export type PanDirection = "leftToRight" | "rightToLeft" | "pingPong";

export type ImageFileConfig = {
  filePath: string;
  panDirection?: PanDirection;       // Default: "leftToRight"
  panDurationSeconds?: number;        // Default: 120 (2 minutes)
  visiblePortion?: number;            // Default: 0.6 (60% of image width visible)
};

export const defaultImagePanSettings = {
  panDirection: "leftToRight" as PanDirection,
  panDurationSeconds: 120,
  visiblePortion: 0.6,
};

export const panDirectionLabels: Record<PanDirection, string> = {
  leftToRight: "Left to Right",
  rightToLeft: "Right to Left",
  pingPong: "Ping Pong (Back & Forth)",
};

export type Feed = {
  projectId: string;
  id: string;
  name: string;
  type: FeedType;
  configJson: VideoFileConfig | ImageFileConfig | Record<string, unknown> | string;
};
