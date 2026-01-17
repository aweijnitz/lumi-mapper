<script setup lang="ts">
import { computed, onMounted, watch, ref } from "vue";
import Tabs from "primevue/tabs";
import TabList from "primevue/tablist";
import Tab from "primevue/tab";
import TabPanels from "primevue/tabpanels";
import TabPanel from "primevue/tabpanel";
import Menubar from "primevue/menubar";
import Dialog from "primevue/dialog";
import InputText from "primevue/inputtext";
import Textarea from "primevue/textarea";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import SceneCanvas from "../components/organisms/SceneCanvas.vue";
import AssetBrowser from "../components/organisms/AssetBrowser.vue";
import FeedBrowser from "../components/organisms/FeedBrowser.vue";
import RendererBrowser from "../components/organisms/RendererBrowser.vue";
import SurfacePropertiesPanel from "../components/organisms/SurfacePropertiesPanel.vue";
import SceneSection from "../components/organisms/SceneSection.vue";
import CueSection from "../components/organisms/CueSection.vue";
import { useProjectStore } from "../stores/projectStore";
import { useSceneStore } from "../stores/sceneStore";
import { useFeedStore } from "../stores/feedStore";
import { useCueStore } from "../stores/cueStore";
import { useAssetStore } from "../stores/assetStore";
import { createId } from "../composables/useIds";
import type { Project } from "../types/project";

const projectStore = useProjectStore();
const sceneStore = useSceneStore();
const feedStore = useFeedStore();
const cueStore = useCueStore();
const assetStore = useAssetStore();

const activeBrowserTab = ref("assets");
const showProjectDialog = ref(false);
const showProjectLoadDialog = ref(false);
const newProjectName = ref("");
const newProjectDescription = ref("");
const selectedProject = ref<Project | null>(null);

const resetProjectData = () => {
  sceneStore.scenes = [];
  sceneStore.activeScene = null;
  sceneStore.activeSurfaceId = null;
  feedStore.feeds = [];
  feedStore.activeFeed = null;
  cueStore.cues = [];
  cueStore.activeCue = null;
};

const canCreateProject = computed(() => newProjectName.value.trim().length > 0);

const resetProjectForm = () => {
  newProjectName.value = "";
  newProjectDescription.value = "";
};

const openProjectLoader = () => {
  selectedProject.value = projectStore.activeProject ?? null;
  showProjectLoadDialog.value = true;
};

const loadSelectedProject = () => {
  if (!selectedProject.value) {
    return;
  }
  projectStore.setActiveProject(selectedProject.value);
  showProjectLoadDialog.value = false;
};

const handleSaveProject = async () => {
  if (!projectStore.activeProject) {
    return;
  }
  const cueOrder = cueStore.cues.length
    ? cueStore.cues.map((cue) => cue.id)
    : projectStore.activeProject.cueOrder;
  const payload = {
    ...projectStore.activeProject,
    cueOrder,
  };
  await projectStore.updateProject(payload);
};

const createProject = async () => {
  const payload: Project = {
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
  await projectStore.createProject(payload);
  resetProjectForm();
  showProjectDialog.value = false;
};

const closeProject = () => {
  projectStore.setActiveProject(null);
  resetProjectData();
};

const projectMenuItems = computed(() => [
  {
    label: "Project",
    items: [
      {
        label: "New Project",
        icon: "pi pi-plus",
        command: () => {
          resetProjectForm();
          showProjectDialog.value = true;
        },
      },
      {
        label: "Save Project",
        icon: "pi pi-save",
        disabled: !projectStore.activeProject,
        command: () => {
          void handleSaveProject();
        },
      },
      {
        label: "Load Project",
        icon: "pi pi-folder-open",
        disabled: projectStore.projects.length === 0,
        command: openProjectLoader,
      },
      {
        label: "Close Project",
        icon: "pi pi-times",
        disabled: !projectStore.activeProject,
        command: closeProject,
      },
    ],
  },
]);

onMounted(() => {
  projectStore.fetchProjects();
  assetStore.fetchAssets();
});

watch(
  () => projectStore.activeProject?.id,
  async (projectId) => {
    if (!projectId) {
      resetProjectData();
      return;
    }
    await Promise.all([
      sceneStore.fetchScenes(projectId),
      feedStore.fetchFeeds(projectId),
      cueStore.fetchCues(projectId),
    ]);
  },
);
</script>

<template>
  <div class="app-shell">
    <header class="app-header">
      <Menubar class="app-header__menu" :model="projectMenuItems" />
      <div class="app-header__title">Projection Composer</div>
    </header>

    <main class="app-main">
      <section class="app-panel app-panel--left">
        <div class="app-panel__header">Browser</div>
        <Tabs v-model:value="activeBrowserTab">
          <TabList>
            <Tab value="scenes">Scenes</Tab>
            <Tab value="cues">Cues</Tab>
            <Tab value="feeds">Feeds</Tab>
            <Tab value="assets">Assets</Tab>
            <Tab value="renderers">Renderers</Tab>
          </TabList>
          <TabPanels>
            <TabPanel value="scenes">
              <SceneSection />
            </TabPanel>
            <TabPanel value="cues">
              <CueSection />
            </TabPanel>
            <TabPanel value="feeds">
              <FeedBrowser />
            </TabPanel>
            <TabPanel value="assets">
              <AssetBrowser />
            </TabPanel>
            <TabPanel value="renderers">
              <RendererBrowser />
            </TabPanel>
          </TabPanels>
        </Tabs>
      </section>

      <section class="app-panel app-panel--center">
        <div class="app-panel__header">Workflow</div>
        <SceneCanvas />
      </section>

      <section class="app-panel app-panel--right">
        <div class="app-panel__header">Properties</div>
        <SurfacePropertiesPanel />
      </section>
    </main>

    <Dialog v-model:visible="showProjectDialog" modal header="New Project">
      <div class="project-dialog">
        <label class="project-dialog__label" for="project-name">Project name</label>
        <InputText id="project-name" v-model="newProjectName" />
        <label class="project-dialog__label" for="project-description">Description</label>
        <Textarea id="project-description" v-model="newProjectDescription" rows="4" />
        <div class="project-dialog__actions">
          <Button label="Create" :disabled="!canCreateProject" @click="createProject" />
          <Button label="Cancel" text @click="showProjectDialog = false" />
        </div>
      </div>
    </Dialog>

    <Dialog v-model:visible="showProjectLoadDialog" modal header="Load Project">
      <div class="project-dialog">
        <DataTable
          :value="projectStore.projects"
          dataKey="id"
          selectionMode="single"
          v-model:selection="selectedProject"
          responsiveLayout="scroll"
          size="small"
        >
          <Column field="name" header="Project" />
          <Column field="description" header="Description" />
        </DataTable>
        <div class="project-dialog__actions">
          <Button label="Load" :disabled="!selectedProject" @click="loadSelectedProject" />
          <Button label="Cancel" text @click="showProjectLoadDialog = false" />
        </div>
      </div>
    </Dialog>
  </div>
</template>

<style scoped>
:global(body) {
  margin: 0;
  background: #0b0c0f;
  color: #e6e2da;
}

.app-shell {
  min-height: 100vh;
  background: radial-gradient(circle at top left, #14161a, #0b0c0f 60%, #07080a);
  color: #e6e2da;
}

.app-header {
  position: sticky;
  top: 0;
  z-index: 10;
  display: flex;
  align-items: center;
  justify-content: space-between;
  min-height: 72px;
  padding: 12px 20px;
  background: linear-gradient(120deg, #1c1f24, #111419);
  border-bottom: 1px solid #22262c;
}

.app-header__menu {
  border: none;
  background: transparent;
  padding: 0;
}

.app-header__title {
  font-size: 1.05rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.14em;
}

.app-main {
  display: grid;
  grid-template-columns: minmax(240px, 280px) minmax(0, 1fr) minmax(240px, 320px);
  gap: 16px;
  padding: 20px;
}

.app-panel {
  background: #14171dbf;
  border: 1px solid #242a31;
  border-radius: 16px;
  padding: 16px;
  min-height: 70vh;
  box-shadow: 0 18px 28px -20px rgba(0, 0, 0, 0.6);
}

.app-panel__header {
  font-size: 0.9rem;
  text-transform: uppercase;
  letter-spacing: 0.12em;
  font-weight: 700;
  margin-bottom: 12px;
}

.app-panel__copy {
  color: #a8a39a;
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

.project-dialog {
  display: flex;
  flex-direction: column;
  gap: 12px;
  min-width: min(380px, 90vw);
}

.project-dialog__label {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #9a958c;
}

.project-dialog__actions {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}
</style>
