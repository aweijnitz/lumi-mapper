import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useProjectStore } from "./projectStore";
import type { Project } from "../types/project";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

const baseProject: Project = {
  id: "project-1",
  name: "Project One",
  description: "Desc",
  createdAt: "2026-02-02T10:00:00Z",
  updatedAt: "2026-02-02T10:00:00Z",
  assetIds: [],
  sceneIds: [],
  feedIds: [],
  cueOrder: [],
  settings: {
    controllers: {},
    midiChannels: [],
    globalConfig: {},
  },
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("fetchProjects stores project list", async () => {
  globalThis.fetch = mockFetch(200, [baseProject]) as unknown as typeof fetch;

  const store = useProjectStore();
  await store.fetchProjects();

  expect(store.projects).toHaveLength(1);
  expect(store.projects[0].id).toBe("project-1");
});

it("createProject sets activeProject", async () => {
  globalThis.fetch = mockFetch(201, baseProject) as unknown as typeof fetch;

  const store = useProjectStore();
  await store.createProject(baseProject);

  expect(store.activeProject?.id).toBe("project-1");
  expect(store.projects).toHaveLength(1);
});
