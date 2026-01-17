export type FeedType = "VideoFile";

export type VideoFileConfig = {
  filePath: string;
};

export type Feed = {
  projectId: string;
  id: string;
  name: string;
  type: FeedType;
  configJson: VideoFileConfig | Record<string, unknown> | string;
};
