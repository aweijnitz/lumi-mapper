import { defineStore } from "pinia";
import type { Cue } from "../types/cue";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

export const useCueStore = defineStore("cues", {
  state: () => ({
    cues: [] as Cue[],
    activeCue: null as Cue | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchCues(projectId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        const cues = await requestJson<Cue[]>(`/api/projects/${projectId}/cues`, {
          method: "GET",
        });
        this.cues = cues ?? [];
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load cues.");
      } finally {
        this.isLoading = false;
      }
    },
    setActiveCue(cue: Cue | null) {
      this.activeCue = cue;
    },
    async createCue(payload: Cue) {
      this.isLoading = true;
      this.error = null;
      try {
        const created = await requestJson<Cue>(`/api/projects/${payload.projectId}/cues`, {
          method: "POST",
          body: JSON.stringify(payload),
        });
        const next = created ?? payload;
        this.cues = [...this.cues, next];
        this.activeCue = next;
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to create cue.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async updateCue(payload: Cue) {
      this.isLoading = true;
      this.error = null;
      try {
        const updated = await requestJson<Cue>(`/api/projects/${payload.projectId}/cues/${payload.id}`, {
          method: "PUT",
          body: JSON.stringify(payload),
        });
        const next = updated ?? payload;
        this.cues = this.cues.map((cue) => (cue.id === next.id ? next : cue));
        if (this.activeCue?.id === next.id) {
          this.activeCue = next;
        }
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to update cue.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async deleteCue(projectId: string, cueId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/projects/${projectId}/cues/${cueId}`, {
          method: "DELETE",
        });
        this.cues = this.cues.filter((cue) => cue.id !== cueId);
        if (this.activeCue?.id === cueId) {
          this.activeCue = null;
        }
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to delete cue.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
  },
});
