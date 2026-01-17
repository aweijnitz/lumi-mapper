import type { Surface } from "./surface";

export type Scene = {
  projectId: string;
  id: string;
  name: string;
  description: string;
  surfaces: Surface[];
};
