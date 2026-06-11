import { defineStore } from "pinia";
import type { Cue } from "../types/cue";
import { requestJson } from "../composables/useApiClient";
import {
  appendEntity,
  clearActiveEntity,
  removeEntity,
  replaceActiveEntity,
  replaceEntity,
  runStoreRequest,
} from "../composables/useStoreCrud";

export const useCueStore = defineStore("cues", {
  state: () => ({
    cues: [] as Cue[],
    activeCue: null as Cue | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchCues(projectId: string) {
      const cues = await runStoreRequest(this, "Failed to load cues.", async () => {
        const cues = await requestJson<Cue[]>(`/api/projects/${projectId}/cues`, {
          method: "GET",
        });
        return cues ?? [];
      });
      if (cues !== undefined) {
        this.cues = cues;
      }
    },
    setActiveCue(cue: Cue | null) {
      this.activeCue = cue;
    },
    async createCue(payload: Cue) {
      return runStoreRequest(
        this,
        "Failed to create cue.",
        async () => {
          const created = await requestJson<Cue>(`/api/projects/${payload.projectId}/cues`, {
            method: "POST",
            body: JSON.stringify(payload),
          });
          const next = created ?? payload;
          this.cues = appendEntity(this.cues, next);
          this.activeCue = next;
          return next;
        },
        { rethrow: true },
      );
    },
    async updateCue(payload: Cue) {
      return runStoreRequest(
        this,
        "Failed to update cue.",
        async () => {
          const updated = await requestJson<Cue>(
            `/api/projects/${payload.projectId}/cues/${payload.id}`,
            {
              method: "PUT",
              body: JSON.stringify(payload),
            },
          );
          const next = updated ?? payload;
          this.cues = replaceEntity(this.cues, next);
          this.activeCue = replaceActiveEntity(this.activeCue, next);
          return next;
        },
        { rethrow: true },
      );
    },
    async deleteCue(projectId: string, cueId: string) {
      await runStoreRequest(
        this,
        "Failed to delete cue.",
        async () => {
          await requestJson(`/api/projects/${projectId}/cues/${cueId}`, {
            method: "DELETE",
          });
          this.cues = removeEntity(this.cues, cueId);
          this.activeCue = clearActiveEntity(this.activeCue, cueId);
        },
        { rethrow: true },
      );
    },
  },
});
