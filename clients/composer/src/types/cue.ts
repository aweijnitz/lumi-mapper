export type SurfaceValue = {
  surfaceId: string;
  value: number;
};

export type Cue = {
  projectId: string;
  id: string;
  name: string;
  sceneId: string;
  surfaceOpacities: SurfaceValue[];
  surfaceBrightnesses: SurfaceValue[];
};
