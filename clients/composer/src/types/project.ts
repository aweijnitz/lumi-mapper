export type ProjectSettings = {
  controllers: Record<string, string>;
  midiChannels: number[];
  globalConfig: Record<string, string>;
};

export type Project = {
  id: string;
  name: string;
  description: string;
  cueOrder: string[];
  settings: ProjectSettings;
};
