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

it("createProject normalizes payload defaults for API", async () => {
  const fetchSpy = vi.fn().mockResolvedValue({
    ok: true,
    status: 201,
    text: vi.fn().mockResolvedValue(JSON.stringify(baseProject)),
    json: vi.fn().mockResolvedValue(baseProject),
  });
  globalThis.fetch = fetchSpy as unknown as typeof fetch;

  const store = useProjectStore();
  await store.createProject({
    id: "project-2",
    name: "Project Two",
    description: "Desc",
    createdAt: "",
    updatedAt: "",
    cueOrder: [],
    settings: {
      controllers: {},
      midiChannels: [],
      globalConfig: {},
    },
  });

  expect(fetchSpy).toHaveBeenCalledTimes(1);
  const [, options] = fetchSpy.mock.calls[0] as [string, RequestInit];
  const body = JSON.parse(String(options.body));
  expect(Array.isArray(body.assetIds)).toBe(true);
  expect(Array.isArray(body.sceneIds)).toBe(true);
  expect(Array.isArray(body.feedIds)).toBe(true);
  expect(Array.isArray(body.cueOrder)).toBe(true);
  expect(typeof body.createdAt).toBe("string");
  expect(body.createdAt.length).toBeGreaterThan(0);
  expect(typeof body.updatedAt).toBe("string");
  expect(body.updatedAt.length).toBeGreaterThan(0);
  expect(body.settings).toEqual({
    controllers: {},
    midiChannels: [],
    globalConfig: {},
  });
});
