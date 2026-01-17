import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useCueStore } from "./cueStore";
import type { Cue } from "../types/cue";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

const baseCue: Cue = {
  projectId: "project-1",
  id: "cue-1",
  name: "Cue",
  sceneId: "scene-1",
  surfaceOpacities: [],
  surfaceBrightnesses: [],
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("fetchCues loads cues for project", async () => {
  globalThis.fetch = mockFetch(200, [baseCue]) as unknown as typeof fetch;

  const store = useCueStore();
  await store.fetchCues("project-1");

  expect(store.cues).toHaveLength(1);
});

it("createCue sets activeCue", async () => {
  globalThis.fetch = mockFetch(201, baseCue) as unknown as typeof fetch;

  const store = useCueStore();
  await store.createCue(baseCue);

  expect(store.activeCue?.id).toBe("cue-1");
});

it("deleteCue removes cue and clears selection", async () => {
  globalThis.fetch = mockFetch(204, "") as unknown as typeof fetch;

  const store = useCueStore();
  store.cues = [baseCue];
  store.activeCue = baseCue;

  await store.deleteCue("project-1", "cue-1");

  expect(store.cues).toHaveLength(0);
  expect(store.activeCue).toBeNull();
});
