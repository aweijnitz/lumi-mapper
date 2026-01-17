import { defineStore } from "pinia";
import type { Feed } from "../types/feed";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

export const useFeedStore = defineStore("feeds", {
  state: () => ({
    feeds: [] as Feed[],
    activeFeed: null as Feed | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchFeeds(projectId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        const feeds = await requestJson<Feed[]>(`/api/projects/${projectId}/feeds`, {
          method: "GET",
        });
        this.feeds = feeds ?? [];
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load feeds.");
      } finally {
        this.isLoading = false;
      }
    },
    setActiveFeed(feed: Feed | null) {
      this.activeFeed = feed;
    },
    async createFeed(payload: Feed) {
      this.isLoading = true;
      this.error = null;
      try {
        const created = await requestJson<Feed>(`/api/projects/${payload.projectId}/feeds`, {
          method: "POST",
          body: JSON.stringify(payload),
        });
        const next = created ?? payload;
        this.feeds = [...this.feeds, next];
        this.activeFeed = next;
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to create feed.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async updateFeed(payload: Feed) {
      this.isLoading = true;
      this.error = null;
      try {
        const updated = await requestJson<Feed>(
          `/api/projects/${payload.projectId}/feeds/${payload.id}`,
          {
            method: "PUT",
            body: JSON.stringify(payload),
          },
        );
        const next = updated ?? payload;
        this.feeds = this.feeds.map((feed) => (feed.id === next.id ? next : feed));
        if (this.activeFeed?.id === next.id) {
          this.activeFeed = next;
        }
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to update feed.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async deleteFeed(projectId: string, feedId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/projects/${projectId}/feeds/${feedId}`, {
          method: "DELETE",
        });
        this.feeds = this.feeds.filter((feed) => feed.id !== feedId);
        if (this.activeFeed?.id === feedId) {
          this.activeFeed = null;
        }
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to delete feed.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
  },
});
