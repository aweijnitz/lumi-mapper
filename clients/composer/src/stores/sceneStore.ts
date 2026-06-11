import { defineStore } from "pinia";
import type { Scene } from "../types/scene";
import { requestJson } from "../composables/useApiClient";
import {
  appendEntity,
  clearActiveEntity,
  removeEntity,
  replaceActiveEntity,
  replaceEntity,
  runStoreRequest,
} from "../composables/useStoreCrud";

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
      const scenes = await runStoreRequest(this, "Failed to load scenes.", async () => {
        const scenes = await requestJson<Scene[]>(`/api/projects/${projectId}/scenes`, {
          method: "GET",
        });
        return scenes ?? [];
      });
      if (scenes !== undefined) {
        this.scenes = scenes;
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
      return runStoreRequest(
        this,
        "Failed to create scene.",
        async () => {
          const created = await requestJson<Scene>(`/api/projects/${payload.projectId}/scenes`, {
            method: "POST",
            body: JSON.stringify(payload),
          });
          const next = created ?? payload;
          this.scenes = appendEntity(this.scenes, next);
          this.activeScene = next;
          return next;
        },
        { rethrow: true },
      );
    },
    async updateScene(payload: Scene) {
      return runStoreRequest(
        this,
        "Failed to update scene.",
        async () => {
          const updated = await requestJson<Scene>(
            `/api/projects/${payload.projectId}/scenes/${payload.id}`,
            {
              method: "PUT",
              body: JSON.stringify(payload),
            },
          );
          const next = updated ?? payload;
          this.scenes = replaceEntity(this.scenes, next);
          this.activeScene = replaceActiveEntity(this.activeScene, next);
          return next;
        },
        { rethrow: true },
      );
    },
    async deleteScene(projectId: string, sceneId: string) {
      await runStoreRequest(
        this,
        "Failed to delete scene.",
        async () => {
          await requestJson(`/api/projects/${projectId}/scenes/${sceneId}`, {
            method: "DELETE",
          });
          const nextActiveScene = clearActiveEntity(this.activeScene, sceneId);
          this.scenes = removeEntity(this.scenes, sceneId);
          this.activeScene = nextActiveScene;
          if (!nextActiveScene) {
            this.activeSurfaceId = null;
          }
        },
        { rethrow: true },
      );
    },
  },
});
