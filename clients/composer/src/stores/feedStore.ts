import { defineStore } from "pinia";
import type { Feed } from "../types/feed";
import { requestJson } from "../composables/useApiClient";
import {
  appendEntity,
  clearActiveEntity,
  removeEntity,
  replaceActiveEntity,
  replaceEntity,
  runStoreRequest,
} from "../composables/useStoreCrud";

export const useFeedStore = defineStore("feeds", {
  state: () => ({
    feeds: [] as Feed[],
    activeFeed: null as Feed | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchFeeds(projectId: string) {
      const feeds = await runStoreRequest(this, "Failed to load feeds.", async () => {
        const feeds = await requestJson<Feed[]>(`/api/projects/${projectId}/feeds`, {
          method: "GET",
        });
        return feeds ?? [];
      });
      if (feeds !== undefined) {
        this.feeds = feeds;
      }
    },
    setActiveFeed(feed: Feed | null) {
      this.activeFeed = feed;
    },
    async createFeed(payload: Feed) {
      return runStoreRequest(
        this,
        "Failed to create feed.",
        async () => {
          const created = await requestJson<Feed>(`/api/projects/${payload.projectId}/feeds`, {
            method: "POST",
            body: JSON.stringify(payload),
          });
          const next = created ?? payload;
          this.feeds = appendEntity(this.feeds, next);
          this.activeFeed = next;
          return next;
        },
        { rethrow: true },
      );
    },
    async updateFeed(payload: Feed) {
      return runStoreRequest(
        this,
        "Failed to update feed.",
        async () => {
          const updated = await requestJson<Feed>(
            `/api/projects/${payload.projectId}/feeds/${payload.id}`,
            {
              method: "PUT",
              body: JSON.stringify(payload),
            },
          );
          const next = updated ?? payload;
          this.feeds = replaceEntity(this.feeds, next);
          this.activeFeed = replaceActiveEntity(this.activeFeed, next);
          return next;
        },
        { rethrow: true },
      );
    },
    async deleteFeed(projectId: string, feedId: string) {
      await runStoreRequest(
        this,
        "Failed to delete feed.",
        async () => {
          await requestJson(`/api/projects/${projectId}/feeds/${feedId}`, {
            method: "DELETE",
          });
          this.feeds = removeEntity(this.feeds, feedId);
          this.activeFeed = clearActiveEntity(this.activeFeed, feedId);
        },
        { rethrow: true },
      );
    },
  },
});
