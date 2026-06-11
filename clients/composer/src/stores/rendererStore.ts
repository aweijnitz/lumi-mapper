import { defineStore } from "pinia";
import { requestJson } from "../composables/useApiClient";
import { runStoreRequest } from "../composables/useStoreCrud";

export type RendererInfo = {
  name: string;
  width: number;
  height: number;
};

export type RendererStatus = {
  status: string;
  renderers: RendererInfo[];
};

export const useRendererStore = defineStore("renderer", {
  state: () => ({
    isLoading: false,
    error: null as string | null,
    lastStatus: null as RendererStatus | null,
    testPatternEnabled: false,
  }),
  getters: {
    primaryRenderer: (state) => state.lastStatus?.renderers?.[0] ?? null,
  },
  actions: {
    async ping() {
      const status = await runStoreRequest(this, "Renderer not available.", async () => {
        const status = await requestJson<RendererStatus>("/api/renderer/ping", {
          method: "GET",
          cache: "no-store",
        });
        return status ?? null;
      });
      if (status !== undefined) {
        this.lastStatus = status;
      }
    },
    async loadScene(projectId: string, sceneId: string) {
      await runStoreRequest(
        this,
        "Failed to load scene.",
        async () => {
          await requestJson(`/api/projects/${projectId}/renderer/loadScene`, {
            method: "POST",
            body: JSON.stringify({ sceneId }),
          });
        },
        { rethrow: true },
      );
    },
    async playCue(projectId: string, cueId: string) {
      await runStoreRequest(
        this,
        "Failed to play cue.",
        async () => {
          await requestJson(`/api/projects/${projectId}/renderer/playCue`, {
            method: "POST",
            body: JSON.stringify({ cueId }),
          });
        },
        { rethrow: true },
      );
    },
    async toggleTestPattern(enabled?: boolean) {
      const newState = enabled ?? !this.testPatternEnabled;
      await runStoreRequest(
        this,
        "Failed to toggle test pattern.",
        async () => {
          await requestJson("/api/renderer/testPattern", {
            method: "POST",
            body: JSON.stringify({ enabled: newState }),
          });
          this.testPatternEnabled = newState;
        },
        { rethrow: true },
      );
    },
    async showCrosshair(enabled: boolean, x = 0, y = 0) {
      // Fire-and-forget: don't set loading state or track errors
      // This is called frequently during drag so must be lightweight
      try {
        await requestJson("/api/renderer/crosshair", {
          method: "POST",
          body: JSON.stringify({ enabled, x, y }),
        });
      } catch {
        // Silently ignore errors - crosshair is a nice-to-have
      }
    },
  },
});
