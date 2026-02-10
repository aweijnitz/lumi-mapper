import { defineStore } from "pinia";
import type { Project } from "../types/project";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

const normalizeProjectPayload = (payload: Project): Project => {
  const now = new Date().toISOString();
  const createdAt = payload.createdAt && payload.createdAt.trim().length > 0 ? payload.createdAt : now;
  const updatedAt = payload.updatedAt && payload.updatedAt.trim().length > 0 ? payload.updatedAt : now;

  return {
    ...payload,
    createdAt,
    updatedAt,
    assetIds: Array.isArray(payload.assetIds) ? payload.assetIds : [],
    sceneIds: Array.isArray(payload.sceneIds) ? payload.sceneIds : [],
    feedIds: Array.isArray(payload.feedIds) ? payload.feedIds : [],
    cueOrder: Array.isArray(payload.cueOrder) ? payload.cueOrder : [],
    settings: {
      controllers: payload.settings?.controllers ?? {},
      midiChannels: Array.isArray(payload.settings?.midiChannels) ? payload.settings.midiChannels : [],
      globalConfig: payload.settings?.globalConfig ?? {},
    },
  };
};

export const useProjectStore = defineStore("projects", {
  state: () => ({
    projects: [] as Project[],
    activeProject: null as Project | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchProjects() {
      this.isLoading = true;
      this.error = null;
      try {
        const projects = await requestJson<Project[]>("/api/projects", { method: "GET" });
        this.projects = projects ?? [];
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load projects.");
      } finally {
        this.isLoading = false;
      }
    },
    setActiveProject(project: Project | null) {
      this.activeProject = project;
    },
    async createProject(payload: Project) {
      this.isLoading = true;
      this.error = null;
      try {
        const normalized = normalizeProjectPayload(payload);
        const created = await requestJson<Project>("/api/projects", {
          method: "POST",
          body: JSON.stringify(normalized),
        });
        const next = created ?? normalized;
        this.projects = [...this.projects, next];
        this.activeProject = next;
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to create project.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async updateProject(payload: Project) {
      this.isLoading = true;
      this.error = null;
      try {
        const normalized = normalizeProjectPayload(payload);
        const updated = await requestJson<Project>(`/api/projects/${payload.id}`, {
          method: "PUT",
          body: JSON.stringify(normalized),
        });
        const next = updated ?? normalized;
        this.projects = this.projects.map((project) =>
          project.id === next.id ? next : project,
        );
        if (this.activeProject?.id === next.id) {
          this.activeProject = next;
        }
        return next;
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to update project.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
  },
});
