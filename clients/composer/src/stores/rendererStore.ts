import { defineStore } from "pinia";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

export type RendererStatus = {
  status: string;
  renderers: string[];
};

export const useRendererStore = defineStore("renderer", {
  state: () => ({
    isLoading: false,
    error: null as string | null,
    lastStatus: null as RendererStatus | null,
  }),
  actions: {
    async ping() {
      this.isLoading = true;
      this.error = null;
      try {
        const status = await requestJson<RendererStatus>("/api/renderer/ping", { method: "GET" });
        this.lastStatus = status ?? null;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Renderer not available.");
      } finally {
        this.isLoading = false;
      }
    },
    async loadScene(projectId: string, sceneId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/projects/${projectId}/renderer/loadScene`, {
          method: "POST",
          body: JSON.stringify({ sceneId }),
        });
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load scene.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async playCue(projectId: string, cueId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/projects/${projectId}/renderer/playCue`, {
          method: "POST",
          body: JSON.stringify({ cueId }),
        });
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to play cue.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
  },
});
