import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useAssetStore } from "./assetStore";
import type { Asset } from "../types/asset";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

const asset: Asset = {
  id: "asset-1",
  name: "Clip A",
  path: "/data/assets/clipA.mp4",
  type: "VideoFile",
  variants: [],
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("fetchAssets loads asset list", async () => {
  globalThis.fetch = mockFetch(200, [asset]) as unknown as typeof fetch;

  const store = useAssetStore();
  await store.fetchAssets();

  expect(store.assets).toHaveLength(1);
});

it("uploadAsset posts form data and refreshes assets", async () => {
  const fetchMock = vi
    .fn()
    .mockResolvedValueOnce({
      ok: true,
      status: 201,
      text: vi.fn().mockResolvedValue(""),
      json: vi.fn().mockResolvedValue({}),
    })
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      text: vi.fn().mockResolvedValue(JSON.stringify([asset])),
      json: vi.fn().mockResolvedValue([asset]),
    });
  globalThis.fetch = fetchMock as unknown as typeof fetch;

  const store = useAssetStore();
  const file = new File(["dummy"], "clip.mp4", { type: "video/mp4" });
  await store.uploadAsset(file);

  expect(fetchMock).toHaveBeenCalledTimes(2);
  const call = fetchMock.mock.calls[0];
  expect(call[0]).toBe("/api/assets");
  expect(call[1]?.method).toBe("POST");
  expect(call[1]?.body).toBeInstanceOf(FormData);
});

it("deleteAsset removes asset and clears selection", async () => {
  globalThis.fetch = mockFetch(204, "") as unknown as typeof fetch;

  const store = useAssetStore();
  store.assets = [asset];
  store.activeAsset = asset;

  await store.deleteAsset(asset.id);

  expect(store.assets).toHaveLength(0);
  expect(store.activeAsset).toBeNull();
});
