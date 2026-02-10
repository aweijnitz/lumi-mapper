export type PanDirection = "leftToRight" | "rightToLeft" | "pingPong";

export type FeedSettings = {
  variantPath?: string;
  monochrome?: boolean;
  panDirection?: PanDirection;
  panDurationSeconds?: number;
  visiblePortion?: number;
};

export const defaultFeedSettings: Required<FeedSettings> = {
  variantPath: "",
  monochrome: false,
  panDirection: "leftToRight",
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
  assetId: string;
  settings?: FeedSettings;
};

