<script setup lang="ts">
import { computed, ref } from "vue";
import Button from "primevue/button";
import Dialog from "primevue/dialog";
import InputText from "primevue/inputtext";
import Menu from "primevue/menu";
import Tab from "primevue/tab";
import TabList from "primevue/tablist";
import TabPanel from "primevue/tabpanel";
import TabPanels from "primevue/tabpanels";
import Tabs from "primevue/tabs";
import Textarea from "primevue/textarea";
import type { MenuItem } from "primevue/menuitem";

type ProjectPayload = {
  id: string;
  name: string;
  description: string;
  cueOrder: string[];
  settings: {
    controllers: Record<string, string>;
    midiChannels: number[];
    globalConfig: Record<string, string>;
  };
};

type FeedSummary = {
  id: string;
  name: string;
};

type SceneSummary = {
  id: string;
  name: string;
};

type CueSummary = {
  id: string;
  name: string;
};

const menu = ref<InstanceType<typeof Menu> | null>(null);
const activeBrowserTab = ref("assets");
const showNewProjectDialog = ref(false);
const newProjectName = ref("");
const newProjectDescription = ref("");
const isCreatingProject = ref(false);
const newProjectError = ref<string | null>(null);
const activeProject = ref<ProjectPayload | null>(null);
const activeFeed = ref<FeedSummary | null>(null);
const activeScene = ref<SceneSummary | null>(null);
const activeCue = ref<CueSummary | null>(null);
const feedName = ref("Clip A");
const sceneName = ref("Main Scene");
const cueName = ref("Cue 1");
const surfaceName = ref("Center Surface");
const assetPath = ref(
  "/Users/aweijnitz/VSCODE_PROJECTS/lumi-mapper/data/assets/clipA.mp4",
);
const isCreatingSetup = ref(false);
const isPlayingCue = ref(false);
const workflowError = ref<string | null>(null);
const lastAction = ref<string | null>(null);

const nameMaxLength = 64;
const descriptionMaxLength = 280;

const nameError = computed(() => {
  const name = newProjectName.value.trim();
  if (!name) {
    return "Project name is required.";
  }
  if (name.length > nameMaxLength) {
    return `Project name must be ${nameMaxLength} characters or less.`;
  }
  return null;
});

const descriptionError = computed(() => {
  const description = newProjectDescription.value.trim();
  if (description.length > descriptionMaxLength) {
    return `Project description must be ${descriptionMaxLength} characters or less.`;
  }
  return null;
});

const canCreateProject = computed(
  () =>
    !nameError.value &&
    !descriptionError.value &&
    !isCreatingProject.value,
);

const canCreateSetup = computed(() => {
  if (!activeProject.value) {
    return false;
  }
  return (
    assetPath.value.trim().length > 0 &&
    feedName.value.trim().length > 0 &&
    sceneName.value.trim().length > 0 &&
    cueName.value.trim().length > 0 &&
    surfaceName.value.trim().length > 0 &&
    !isCreatingSetup.value
  );
});

const canPlayCue = computed(
  () => !!activeProject.value && !!activeCue.value && !isPlayingCue.value,
);

const menuItems: MenuItem[] = [
  {
    label: "New Project",
    command: () => {
      console.log("[menu] new project");
      openNewProjectDialog();
    },
  },
  {
    label: "Load Project",
    command: () => {
      console.log("[menu] load project");
    },
  },
  {
    label: "Save Project",
    command: () => {
      console.log("[menu] save project");
    },
  },
];

const toggleMenu = (event: Event) => {
  menu.value?.toggle(event);
};

const openNewProjectDialog = () => {
  newProjectName.value = "";
  newProjectDescription.value = "";
  newProjectError.value = null;
  showNewProjectDialog.value = true;
};

const closeNewProjectDialog = () => {
  showNewProjectDialog.value = false;
};

const createProject = async () => {
  if (!canCreateProject.value) {
    return;
  }

  isCreatingProject.value = true;
  newProjectError.value = null;

  const payload = {
    id: createId("project"),
    name: newProjectName.value.trim(),
    description: newProjectDescription.value.trim(),
    cueOrder: [],
    settings: {
      controllers: {},
      midiChannels: [],
      globalConfig: {},
    },
  };

  try {
    const created = await requestJson<ProjectPayload>("/api/projects", {
      method: "POST",
      body: JSON.stringify(payload),
    });

    activeProject.value = created ?? payload;
    activeFeed.value = null;
    activeScene.value = null;
    activeCue.value = null;
    lastAction.value = `Created project ${activeProject.value.id}`;
    workflowError.value = null;
    showNewProjectDialog.value = false;
  } catch (error) {
    newProjectError.value =
      error instanceof Error ? error.message : "Failed to create project.";
  } finally {
    isCreatingProject.value = false;
  }
};

const createId = (prefix: string) => `${prefix}-${createRandomId()}`;

const createRandomId = () => {
  if (typeof crypto !== "undefined" && "randomUUID" in crypto) {
    return crypto.randomUUID();
  }
  return `${Date.now()}-${Math.random().toString(16).slice(2)}`;
};

const apiBase = import.meta.env.VITE_API_BASE ?? "";

const requestJson = async <T>(url: string, options: RequestInit) => {
  const requestUrl = apiBase ? new URL(url, apiBase).toString() : url;
  const response = await fetch(requestUrl, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers ?? {}),
    },
  });

  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || "Request failed.");
  }

  return (await response.json()) as T;
};

const createSetup = async () => {
  if (!canCreateSetup.value || !activeProject.value) {
    return;
  }

  isCreatingSetup.value = true;
  workflowError.value = null;

  const projectId = activeProject.value.id;
  const feedId = createId("feed");
  const sceneId = createId("scene");
  const cueId = createId("cue");
  const surfaceId = createId("surface");

  try {
    const feedPayload = {
      projectId,
      id: feedId,
      name: feedName.value.trim(),
      type: "VideoFile",
      configJson: {
        filePath: assetPath.value.trim(),
      },
    };
    const createdFeed = await requestJson<FeedSummary>(
      `/api/projects/${projectId}/feeds`,
      {
        method: "POST",
        body: JSON.stringify(feedPayload),
      },
    );

    const surface = {
      id: surfaceId,
      name: surfaceName.value.trim(),
      vertices: [
        { x: -0.5, y: -0.4 },
        { x: 0.5, y: -0.4 },
        { x: 0.5, y: 0.4 },
        { x: -0.5, y: 0.4 },
      ],
      feedId: createdFeed.id ?? feedId,
      opacity: 1,
      brightness: 1,
      blendMode: "Normal",
      zOrder: 0,
    };

    const scenePayload = {
      projectId,
      id: sceneId,
      name: sceneName.value.trim(),
      description: "",
      surfaces: [surface],
    };
    const createdScene = await requestJson<SceneSummary>(
      `/api/projects/${projectId}/scenes`,
      {
        method: "POST",
        body: JSON.stringify(scenePayload),
      },
    );

    const cuePayload = {
      projectId,
      id: cueId,
      name: cueName.value.trim(),
      sceneId: createdScene.id ?? sceneId,
      surfaceOpacities: [],
      surfaceBrightnesses: [],
    };
    const createdCue = await requestJson<CueSummary>(
      `/api/projects/${projectId}/cues`,
      {
        method: "POST",
        body: JSON.stringify(cuePayload),
      },
    );

    const updatedProject = {
      ...activeProject.value,
      cueOrder: [...activeProject.value.cueOrder, createdCue.id ?? cueId],
    };
    const savedProject = await requestJson<ProjectPayload>(
      `/api/projects/${projectId}`,
      {
        method: "PUT",
        body: JSON.stringify(updatedProject),
      },
    );

    activeProject.value = savedProject ?? updatedProject;
    activeFeed.value = {
      id: createdFeed.id ?? feedId,
      name: createdFeed.name ?? feedPayload.name,
    };
    activeScene.value = {
      id: createdScene.id ?? sceneId,
      name: createdScene.name ?? scenePayload.name,
    };
    activeCue.value = {
      id: createdCue.id ?? cueId,
      name: createdCue.name ?? cuePayload.name,
    };
    lastAction.value = "Created feed, scene, and cue.";
  } catch (error) {
    workflowError.value =
      error instanceof Error ? error.message : "Failed to create setup.";
  } finally {
    isCreatingSetup.value = false;
  }
};

const playCue = async () => {
  if (!canPlayCue.value || !activeProject.value || !activeCue.value) {
    return;
  }

  isPlayingCue.value = true;
  workflowError.value = null;

  try {
    await requestJson(`/api/projects/${activeProject.value.id}/renderer/playCue`, {
      method: "POST",
      body: JSON.stringify({ cueId: activeCue.value.id }),
    });
    lastAction.value = `Sent cue ${activeCue.value.id} to renderer.`;
  } catch (error) {
    workflowError.value =
      error instanceof Error ? error.message : "Failed to play cue.";
  } finally {
    isPlayingCue.value = false;
  }
};
</script>

<template>
  <div class="app-shell">
    <header class="app-header">
      <div class="app-header__left">
        <Button
          class="app-header__button"
          label="Project"
          icon="pi pi-angle-down"
          iconPos="right"
          text
          @click="toggleMenu"
        />
        <Menu ref="menu" :model="menuItems" popup />
      </div>
    </header>

    <main class="app-main">
      <section class="app-panel app-panel--left">
        <div class="app-panel__header">Browser</div>
        <Tabs v-model:value="activeBrowserTab">
          <TabList>
            <Tab value="assets">Assets</Tab>
            <Tab value="scenes">Scenes</Tab>
            <Tab value="renders">Renders</Tab>
          </TabList>
          <TabPanels>
            <TabPanel value="assets">
              <p class="app-panel__copy">
                Asset list and import tools will live here.
              </p>
            </TabPanel>
            <TabPanel value="scenes">
              <p class="app-panel__copy">
                Scene browser and cue stack will live here.
              </p>
            </TabPanel>
            <TabPanel value="renders">
              <p class="app-panel__copy">
                Renderer connections and outputs will live here.
              </p>
            </TabPanel>
          </TabPanels>
        </Tabs>
      </section>

      <section class="app-panel app-panel--center">
        <div class="app-panel__header">Quickstart</div>
        <div class="quickstart">
          <div class="quickstart__row">
            <span class="quickstart__label">Active project</span>
            <span v-if="activeProject" class="quickstart__value">
              {{ activeProject.name }} ({{ activeProject.id }})
            </span>
            <span v-else class="quickstart__value">
              Create a project to continue.
            </span>
          </div>

          <div class="quickstart__grid">
            <div class="quickstart__field">
              <label class="quickstart__label" for="asset-path">
                Asset path
              </label>
              <InputText
                id="asset-path"
                v-model="assetPath"
                placeholder="Absolute asset path"
                autocomplete="off"
              />
            </div>

            <div class="quickstart__field">
              <label class="quickstart__label" for="feed-name">
                Feed name
              </label>
              <InputText
                id="feed-name"
                v-model="feedName"
                placeholder="Feed name"
                autocomplete="off"
              />
            </div>

            <div class="quickstart__field">
              <label class="quickstart__label" for="scene-name">
                Scene name
              </label>
              <InputText
                id="scene-name"
                v-model="sceneName"
                placeholder="Scene name"
                autocomplete="off"
              />
            </div>

            <div class="quickstart__field">
              <label class="quickstart__label" for="surface-name">
                Surface name
              </label>
              <InputText
                id="surface-name"
                v-model="surfaceName"
                placeholder="Surface name"
                autocomplete="off"
              />
            </div>

            <div class="quickstart__field">
              <label class="quickstart__label" for="cue-name">
                Cue name
              </label>
              <InputText
                id="cue-name"
                v-model="cueName"
                placeholder="Cue name"
                autocomplete="off"
              />
            </div>
          </div>

          <div class="quickstart__actions">
            <Button
              label="Create Feed + Scene + Cue"
              data-testid="quick-create"
              :loading="isCreatingSetup"
              :disabled="!canCreateSetup"
              @click="createSetup"
            />
            <Button
              label="Play Cue"
              data-testid="play-cue"
              :loading="isPlayingCue"
              :disabled="!canPlayCue"
              @click="playCue"
            />
          </div>

          <div class="quickstart__summary" v-if="activeFeed || activeScene || activeCue">
            <div>
              <strong>Feed:</strong>
              {{ activeFeed ? `${activeFeed.name} (${activeFeed.id})` : "—" }}
            </div>
            <div>
              <strong>Scene:</strong>
              {{ activeScene ? `${activeScene.name} (${activeScene.id})` : "—" }}
            </div>
            <div>
              <strong>Cue:</strong>
              {{ activeCue ? `${activeCue.name} (${activeCue.id})` : "—" }}
            </div>
          </div>

          <p v-if="workflowError" class="quickstart__error" role="alert">
            {{ workflowError }}
          </p>
          <p v-if="lastAction" class="quickstart__status">
            {{ lastAction }}
          </p>
        </div>
      </section>

      <section class="app-panel app-panel--right">
        <div class="app-panel__header">Properties</div>
        <p class="app-panel__copy">
          Object-level properties will appear here based on the active
          selection.
        </p>
      </section>
    </main>

    <Dialog
      v-model:visible="showNewProjectDialog"
      modal
      header="New Project"
      class="project-dialog"
      @hide="closeNewProjectDialog"
    >
      <div class="project-dialog__form">
        <label class="project-dialog__label" for="project-name">
          Project name
        </label>
        <InputText
          id="project-name"
          v-model="newProjectName"
          placeholder="Project name"
          autocomplete="off"
          :maxlength="nameMaxLength"
          :aria-invalid="!!nameError"
        />
        <p v-if="nameError" class="project-dialog__field-error" role="alert">
          {{ nameError }}
        </p>

        <label class="project-dialog__label" for="project-description">
          Project description
        </label>
        <Textarea
          id="project-description"
          v-model="newProjectDescription"
          placeholder="Project description"
          rows="4"
          autoResize
          :maxlength="descriptionMaxLength"
          :aria-invalid="!!descriptionError"
        />
        <p
          v-if="descriptionError"
          class="project-dialog__field-error"
          role="alert"
        >
          {{ descriptionError }}
        </p>

        <p v-if="newProjectError" class="project-dialog__error" role="alert">
          {{ newProjectError }}
        </p>
      </div>

      <template #footer>
        <Button label="Cancel" text @click="closeNewProjectDialog" />
        <Button
          label="Create"
          :loading="isCreatingProject"
          :disabled="!canCreateProject"
          @click="createProject"
        />
      </template>
    </Dialog>
  </div>
</template>

<style scoped>
:global(body) {
  margin: 0;
}

.app-shell {
  min-height: 100vh;
  background: radial-gradient(circle at top left, #fdfbf7, #f4f1eb 55%, #ebe6dd);
  color: #2f2d2a;
}

.app-header {
  position: sticky;
  top: 0;
  z-index: 10;
  display: flex;
  align-items: center;
  min-height: 72px;
  padding: 12px 20px;
  background: linear-gradient(120deg, #f6f4ef, #e9efe9);
  border-bottom: 1px solid #d5d4cf;
}

.app-header__left {
  display: flex;
  align-items: center;
}

.app-header__button {
  font-size: 0.95rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #2f2d2a;
}

.app-main {
  display: grid;
  grid-template-columns: minmax(240px, 280px) minmax(0, 1fr) minmax(240px, 320px);
  gap: 16px;
  padding: 20px;
}

.app-panel {
  background: #ffffffc9;
  border: 1px solid #d8d4cb;
  border-radius: 16px;
  padding: 16px;
  min-height: 70vh;
  box-shadow: 0 12px 24px -18px rgba(62, 52, 38, 0.45);
}

.app-panel__header {
  font-size: 0.9rem;
  text-transform: uppercase;
  letter-spacing: 0.12em;
  font-weight: 700;
  margin-bottom: 12px;
}

.app-panel__copy {
  margin: 0;
  color: #4a4640;
  line-height: 1.5;
}

@media (max-width: 980px) {
  .app-main {
    grid-template-columns: 1fr;
  }

  .app-panel {
    min-height: auto;
  }
}

.project-dialog__form {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.project-dialog__label {
  font-size: 0.85rem;
  font-weight: 600;
  color: #3b3730;
}

.project-dialog__field-error {
  margin: -6px 0 0;
  color: #a3362a;
  font-size: 0.85rem;
  font-weight: 600;
}

.project-dialog__error {
  margin: 0;
  color: #a3362a;
  font-weight: 600;
}

.quickstart {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.quickstart__row {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.quickstart__label {
  font-size: 0.8rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: #4b463e;
}

.quickstart__value {
  font-size: 0.95rem;
  color: #2f2d2a;
}

.quickstart__grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 12px;
}

.quickstart__field {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.quickstart__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
}

.quickstart__summary {
  border: 1px dashed #c9c1b3;
  border-radius: 12px;
  padding: 12px;
  font-size: 0.9rem;
  color: #4a4640;
  display: grid;
  gap: 6px;
}

.quickstart__error {
  margin: 0;
  color: #a3362a;
  font-weight: 600;
}

.quickstart__status {
  margin: 0;
  color: #3b6f4b;
  font-weight: 600;
}
</style>
