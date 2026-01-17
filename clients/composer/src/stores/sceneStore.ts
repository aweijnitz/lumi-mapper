import { defineStore } from "pinia";
import type { Scene } from "../types/scene";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

export const useSceneStore = defineStore("scenes", {
  state: () => ({
    scenes: [] as Scene[],
    activeScene: null as Scene | null,
    activeSurfaceId: null as string | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchScenes(projectId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        const scenes = await requestJson<Scene[]>(`/api/projects/${projectId}/scenes`, {
          method: "GET",
        });
        this.scenes = scenes ?? [];
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load scenes.");
      } finally {
        this.isLoading = false;
      }
    },
    setActiveScene(scene: Scene | null) {
      this.activeScene = scene;
      this.activeSurfaceId = null;
    },
    setActiveSurfaceId(surfaceId: string | null) {
      this.activeSurfaceId = surfaceId;
    },
    async createScene(payload: Scene) {
      this.isLoading = true;
      this.error = null;
      try {
        const created = await requestJson<Scene>(`/api/projects/${payload.projectId}/scenes`, {
          method: "POST",
          body: JSON.stringify(payload),
        });
        const next = created ?? payload;
        this.scenes = [...this.scenes, next];
        this.activeScene = next;
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to create scene.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async updateScene(payload: Scene) {
      this.isLoading = true;
      this.error = null;
      try {
        const updated = await requestJson<Scene>(
          `/api/projects/${payload.projectId}/scenes/${payload.id}`,
          {
            method: "PUT",
            body: JSON.stringify(payload),
          },
        );
        const next = updated ?? payload;
        this.scenes = this.scenes.map((scene) => (scene.id === next.id ? next : scene));
        if (this.activeScene?.id === next.id) {
          this.activeScene = next;
        }
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to update scene.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async deleteScene(projectId: string, sceneId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/projects/${projectId}/scenes/${sceneId}`, {
          method: "DELETE",
        });
        this.scenes = this.scenes.filter((scene) => scene.id !== sceneId);
        if (this.activeScene?.id === sceneId) {
          this.activeScene = null;
          this.activeSurfaceId = null;
        }
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to delete scene.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
  },
});
