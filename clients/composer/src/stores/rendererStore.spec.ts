import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useRendererStore } from "./rendererStore";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("ping updates lastStatus", async () => {
  globalThis.fetch = mockFetch(200, { status: "ok", renderers: ["main"] }) as unknown as typeof fetch;

  const store = useRendererStore();
  await store.ping();

  expect(store.lastStatus?.status).toBe("ok");
});

it("playCue calls API", async () => {
  globalThis.fetch = mockFetch(200, { status: "sent" }) as unknown as typeof fetch;

  const store = useRendererStore();
  await store.playCue("project-1", "cue-1");

  expect(store.error).toBeNull();
});

it("loadScene calls API", async () => {
  globalThis.fetch = mockFetch(200, { status: "sent" }) as unknown as typeof fetch;

  const store = useRendererStore();
  await store.loadScene("project-1", "scene-1");

  expect(store.error).toBeNull();
});
