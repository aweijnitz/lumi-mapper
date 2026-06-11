import { defineStore } from "pinia";
import type { Project } from "../types/project";
import { requestJson } from "../composables/useApiClient";
import {
  appendEntity,
  replaceActiveEntity,
  replaceEntity,
  runStoreRequest,
} from "../composables/useStoreCrud";
import { normalizeProjectPayload } from "../composables/useProjectPayloadNormalizer";

export const useProjectStore = defineStore("projects", {
  state: () => ({
    projects: [] as Project[],
    activeProject: null as Project | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchProjects() {
      const projects = await runStoreRequest(this, "Failed to load projects.", async () => {
        const projects = await requestJson<Project[]>("/api/projects", { method: "GET" });
        return projects ?? [];
      });
      if (projects !== undefined) {
        this.projects = projects;
      }
    },
    setActiveProject(project: Project | null) {
      this.activeProject = project;
    },
    async createProject(payload: Project) {
      return runStoreRequest(
        this,
        "Failed to create project.",
        async () => {
          const normalized = normalizeProjectPayload(payload);
          const created = await requestJson<Project>("/api/projects", {
            method: "POST",
            body: JSON.stringify(normalized),
          });
          const next = created ?? normalized;
          this.projects = appendEntity(this.projects, next);
          this.activeProject = next;
          return next;
        },
        { rethrow: true },
      );
    },
    async updateProject(payload: Project) {
      return runStoreRequest(
        this,
        "Failed to update project.",
        async () => {
          const normalized = normalizeProjectPayload(payload);
          const updated = await requestJson<Project>(`/api/projects/${payload.id}`, {
            method: "PUT",
            body: JSON.stringify(normalized),
          });
          const next = updated ?? normalized;
          this.projects = replaceEntity(this.projects, next);
          this.activeProject = replaceActiveEntity(this.activeProject, next);
          return next;
        },
        { rethrow: true },
      );
    },
  },
});
