import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useSceneStore } from "./sceneStore";
import type { Scene } from "../types/scene";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

const baseScene: Scene = {
  projectId: "project-1",
  id: "scene-1",
  name: "Scene",
  description: "",
  surfaces: [],
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("fetchScenes loads scenes for project", async () => {
  globalThis.fetch = mockFetch(200, [baseScene]) as unknown as typeof fetch;

  const store = useSceneStore();
  await store.fetchScenes("project-1");

  expect(store.scenes).toHaveLength(1);
});

it("createScene sets activeScene", async () => {
  globalThis.fetch = mockFetch(201, baseScene) as unknown as typeof fetch;

  const store = useSceneStore();
  await store.createScene(baseScene);

  expect(store.activeScene?.id).toBe("scene-1");
});

it("deleteScene removes scene and clears selection", async () => {
  globalThis.fetch = mockFetch(204, "") as unknown as typeof fetch;

  const store = useSceneStore();
  store.scenes = [baseScene];
  store.activeScene = baseScene;
  store.activeSurfaceId = "surface-1";

  await store.deleteScene("project-1", "scene-1");

  expect(store.scenes).toHaveLength(0);
  expect(store.activeScene).toBeNull();
  expect(store.activeSurfaceId).toBeNull();
});
