import { defineStore } from "pinia";
import type { Project } from "../types/project";
import { requestJson, resolveErrorMessage } from "../composables/useApiClient";

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
        const created = await requestJson<Project>("/api/projects", {
          method: "POST",
          body: JSON.stringify(payload),
        });
        const next = created ?? payload;
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
        const updated = await requestJson<Project>(`/api/projects/${payload.id}`, {
          method: "PUT",
          body: JSON.stringify(payload),
        });
        const next = updated ?? payload;
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
