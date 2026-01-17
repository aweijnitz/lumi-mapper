import { beforeEach, expect, it, vi } from "vitest";
import { setActivePinia, createPinia } from "pinia";
import { useFeedStore } from "./feedStore";
import type { Feed } from "../types/feed";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

const baseFeed: Feed = {
  projectId: "project-1",
  id: "feed-1",
  name: "Feed",
  type: "VideoFile",
  configJson: { filePath: "/tmp/clip.mp4" },
};

beforeEach(() => {
  setActivePinia(createPinia());
});

it("fetchFeeds loads feeds for project", async () => {
  globalThis.fetch = mockFetch(200, [baseFeed]) as unknown as typeof fetch;

  const store = useFeedStore();
  await store.fetchFeeds("project-1");

  expect(store.feeds).toHaveLength(1);
});

it("createFeed sets activeFeed", async () => {
  globalThis.fetch = mockFetch(201, baseFeed) as unknown as typeof fetch;

  const store = useFeedStore();
  await store.createFeed(baseFeed);

  expect(store.activeFeed?.id).toBe("feed-1");
});

it("deleteFeed removes feed and clears selection", async () => {
  globalThis.fetch = mockFetch(204, "") as unknown as typeof fetch;

  const store = useFeedStore();
  store.feeds = [baseFeed];
  store.activeFeed = baseFeed;

  await store.deleteFeed("project-1", "feed-1");

  expect(store.feeds).toHaveLength(0);
  expect(store.activeFeed).toBeNull();
});
