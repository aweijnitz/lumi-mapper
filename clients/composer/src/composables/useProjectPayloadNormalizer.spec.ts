import { expect, it } from "vitest";
import { normalizeProjectPayload } from "./useProjectPayloadNormalizer";
import type { Project } from "../types/project";

it("normalizes missing project defaults", () => {
  const now = "2026-06-11T10:20:30.000Z";
  const project = {
    id: "project-1",
    name: "Project One",
    description: "Desc",
    createdAt: "",
    updatedAt: "",
    cueOrder: [],
    settings: {
      controllers: {},
      midiChannels: [],
      globalConfig: {},
    },
  } as Project;

  expect(normalizeProjectPayload(project, now)).toEqual({
    ...project,
    createdAt: now,
    updatedAt: now,
    assetIds: [],
    sceneIds: [],
    feedIds: [],
    cueOrder: [],
    settings: {
      controllers: {},
      midiChannels: [],
      globalConfig: {},
    },
  });
});

it("preserves populated project fields", () => {
  const project: Project = {
    id: "project-2",
    name: "Project Two",
    description: "Desc",
    createdAt: "2026-02-02T10:00:00Z",
    updatedAt: "2026-03-03T11:00:00Z",
    assetIds: ["asset-1"],
    sceneIds: ["scene-1"],
    feedIds: ["feed-1"],
    cueOrder: ["cue-1"],
    settings: {
      controllers: { brightness: "cc1" },
      midiChannels: [1, 2],
      globalConfig: { mode: "show" },
    },
  };

  expect(normalizeProjectPayload(project, "ignored")).toEqual(project);
});
