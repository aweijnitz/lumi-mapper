<script setup lang="ts">
import { computed, onMounted, onUnmounted, watch, ref } from "vue";
import Tabs from "primevue/tabs";
import TabList from "primevue/tablist";
import Tab from "primevue/tab";
import TabPanels from "primevue/tabpanels";
import TabPanel from "primevue/tabpanel";
import Dialog from "primevue/dialog";
import InputText from "primevue/inputtext";
import Textarea from "primevue/textarea";
import Button from "primevue/button";
import Menu from "primevue/menu";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import Splitter from "primevue/splitter";
import SplitterPanel from "primevue/splitterpanel";
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
import { useRendererStore } from "../stores/rendererStore";
import { createId } from "../composables/useIds";
import type { Project } from "../types/project";

const projectStore = useProjectStore();
const sceneStore = useSceneStore();
const feedStore = useFeedStore();
const cueStore = useCueStore();
const assetStore = useAssetStore();
const rendererStore = useRendererStore();

const activeBrowserTab = ref("assets");
const showProjectDialog = ref(false);
const showProjectLoadDialog = ref(false);
const newProjectName = ref("");
const newProjectDescription = ref("");
const selectedProject = ref<Project | null>(null);

// Panel sizing with localStorage persistence
const PANEL_SIZES_KEY = "lumi-panel-sizes";
const DEFAULT_PANEL_SIZES = [20, 60, 20]; // left, center, right as percentages

const loadPanelSizes = (): number[] => {
  try {
    const stored = localStorage.getItem(PANEL_SIZES_KEY);
    if (stored) {
      const sizes = JSON.parse(stored);
      if (Array.isArray(sizes) && sizes.length === 3 && sizes.every((s) => typeof s === "number")) {
        return sizes;
      }
    }
  } catch {
    // ignore parse errors
  }
  return DEFAULT_PANEL_SIZES;
};

const panelSizes = ref(loadPanelSizes());

const savePanelSizes = () => {
  try {
    localStorage.setItem(PANEL_SIZES_KEY, JSON.stringify(panelSizes.value));
  } catch {
    // ignore storage errors
  }
};

const onPanelResize = (event: { sizes: number[] }) => {
  panelSizes.value = event.sizes;
  savePanelSizes();
};

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

const onProjectRowSelect = (event: { data: Project }) => {
  projectStore.setActiveProject(event.data);
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

// Project menu for the header
const projectMenuRef = ref<InstanceType<typeof Menu> | null>(null);

const projectMenuItems = computed(() => {
  const items = [];

  // Show recent projects at the top for quick access
  if (projectStore.projects.length > 0) {
    const recentProjects = projectStore.projects.slice(0, 5).map((project) => ({
      label: project.name,
      icon: project.id === projectStore.activeProject?.id ? "pi pi-check" : "pi pi-folder",
      command: () => {
        projectStore.setActiveProject(project);
      },
    }));
    items.push(...recentProjects);
    items.push({ separator: true });
  }

  items.push(
    {
      label: "New Project",
      icon: "pi pi-plus",
      command: () => {
        resetProjectForm();
        showProjectDialog.value = true;
      },
    },
    {
      label: "Browse All Projects",
      icon: "pi pi-list",
      disabled: projectStore.projects.length === 0,
      command: openProjectLoader,
    }
  );

  if (projectStore.activeProject) {
    items.push(
      { separator: true },
      {
        label: "Save Project",
        icon: "pi pi-save",
        command: () => {
          void handleSaveProject();
        },
      },
      {
        label: "Close Project",
        icon: "pi pi-times",
        command: closeProject,
      }
    );
  }

  return items;
});

const toggleProjectMenu = (event: Event) => {
  projectMenuRef.value?.toggle(event);
};

// Connection status tracking
const apiConnected = ref(false);
const pingInterval = ref<ReturnType<typeof setInterval> | null>(null);

const connectedRendererCount = computed(() => rendererStore.lastStatus?.renderers?.length ?? 0);

const connectionStatus = computed(() => {
  if (rendererStore.isLoading) {
    return { state: "checking", label: "Checking...", color: "orange" };
  }
  if (rendererStore.error || !apiConnected.value) {
    return { state: "disconnected", label: "Offline", color: "red" };
  }
  if (connectedRendererCount.value > 0) {
    const count = connectedRendererCount.value;
    return { state: "connected", label: `${count} Renderer${count > 1 ? "s" : ""}`, color: "green" };
  }
  return { state: "ready", label: "Ready", color: "green" };
});

const checkConnection = async () => {
  try {
    await rendererStore.ping();
    apiConnected.value = !rendererStore.error;
  } catch {
    apiConnected.value = false;
  }
};

onMounted(() => {
  projectStore.fetchProjects();
  assetStore.fetchAssets();

  // Initial connection check and periodic polling
  checkConnection();
  pingInterval.value = setInterval(checkConnection, 10000); // Every 10 seconds
});

onUnmounted(() => {
  if (pingInterval.value) {
    clearInterval(pingInterval.value);
  }
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

    // Auto-select the first scene if available
    if (sceneStore.scenes.length > 0) {
      sceneStore.setActiveScene(sceneStore.scenes[0]);
      activeBrowserTab.value = "scenes";
    }
  },
);
</script>

<template>
  <div class="app-shell">
    <header class="app-header">
      <div class="app-header__left">
        <div class="app-header__logo">
          <span class="app-header__logo-text">LUMI</span>
        </div>
        <template v-if="projectStore.activeProject">
          <div class="app-header__divider"></div>
          <button class="app-header__project-btn" @click="toggleProjectMenu" title="Switch project">
            <span class="app-header__project-name">
              {{ projectStore.activeProject.name }}
            </span>
            <i class="pi pi-chevron-down app-header__project-arrow"></i>
          </button>
          <Menu ref="projectMenuRef" :model="projectMenuItems" :popup="true" class="app-header__project-menu" />
        </template>
      </div>
      <div class="app-header__center">
        <Button
          v-if="projectStore.activeProject"
          icon="pi pi-save"
          text
          size="small"
          class="app-header__action"
          title="Save project"
          @click="handleSaveProject"
        />
      </div>
      <div class="app-header__right">
        <label
          class="app-header__toggle"
          :class="{
            'app-header__toggle--active': rendererStore.testPatternEnabled,
            'app-header__toggle--disabled': connectionStatus.state === 'disconnected' || rendererStore.isLoading
          }"
          :title="rendererStore.testPatternEnabled ? 'Hide calibration grid on renderer output' : 'Show calibration grid on renderer output for alignment'"
        >
          <input
            type="checkbox"
            :checked="rendererStore.testPatternEnabled"
            :disabled="connectionStatus.state === 'disconnected' || rendererStore.isLoading"
            @change="rendererStore.toggleTestPattern()"
          />
          <span class="app-header__toggle-track">
            <span class="app-header__toggle-thumb"></span>
          </span>
          <span class="app-header__toggle-label">Output Grid</span>
        </label>
        <div
          class="app-header__connection"
          :class="{
            'app-header__connection--connected': connectionStatus.state === 'connected' || connectionStatus.state === 'ready',
            'app-header__connection--checking': connectionStatus.state === 'checking',
            'app-header__connection--offline': connectionStatus.state === 'disconnected',
          }"
          :title="connectionStatus.state === 'disconnected' ? 'Server not reachable' : connectionStatus.state === 'connected' ? 'Renderer connected' : 'Server online'"
        >
          <span class="app-header__connection-led"></span>
          <span class="app-header__connection-label">{{ connectionStatus.label }}</span>
        </div>
      </div>
    </header>

    <!-- Welcome screen when no project is open -->
    <div v-if="!projectStore.activeProject" class="app-welcome">
      <div class="app-welcome__content">
        <div class="app-welcome__logo">LUMI</div>
        <p class="app-welcome__tagline">Projection Mapping Composer</p>

        <div class="app-welcome__actions">
          <Button
            label="New Project"
            icon="pi pi-plus"
            class="app-welcome__btn app-welcome__btn--primary"
            @click="resetProjectForm(); showProjectDialog = true"
          />
          <Button
            label="Open Project"
            icon="pi pi-folder-open"
            class="app-welcome__btn"
            :disabled="projectStore.projects.length === 0"
            @click="openProjectLoader"
          />
        </div>

        <div v-if="projectStore.projects.length > 0" class="app-welcome__recent">
          <div class="app-welcome__recent-title">Recent Projects</div>
          <div class="app-welcome__recent-list">
            <button
              v-for="project in projectStore.projects.slice(0, 5)"
              :key="project.id"
              class="app-welcome__recent-item"
              @click="projectStore.setActiveProject(project)"
            >
              <i class="pi pi-folder"></i>
              <span class="app-welcome__recent-name">{{ project.name }}</span>
              <span v-if="project.description" class="app-welcome__recent-desc">{{ project.description }}</span>
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- Main workspace when project is open -->
    <Splitter v-else class="app-main" @resizeend="onPanelResize">
      <SplitterPanel :size="panelSizes[0]" :minSize="10" class="app-panel app-panel--left">
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
      </SplitterPanel>

      <SplitterPanel :size="panelSizes[1]" :minSize="20" class="app-panel app-panel--center">
        <div class="app-panel__header">Workflow</div>
        <SceneCanvas />
      </SplitterPanel>

      <SplitterPanel :size="panelSizes[2]" :minSize="10" class="app-panel app-panel--right">
        <div class="app-panel__header">Properties</div>
        <SurfacePropertiesPanel />
      </SplitterPanel>
    </Splitter>

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
        <p class="project-dialog__hint">Click a project to load it</p>
        <DataTable
          :value="projectStore.projects"
          dataKey="id"
          selectionMode="single"
          v-model:selection="selectedProject"
          responsiveLayout="scroll"
          size="small"
          @row-select="onProjectRowSelect"
        >
          <Column field="name" header="Project" />
          <Column field="description" header="Description" />
        </DataTable>
        <div class="project-dialog__actions">
          <Button label="Cancel" text @click="showProjectLoadDialog = false" />
        </div>
      </div>
    </Dialog>
  </div>
</template>

<style scoped>
/* MadMapper-inspired dark theme - Professional polish */
:global(body) {
  margin: 0;
  background: #1a1a1a;
  color: #d0d0d0;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', sans-serif;
  font-size: 14px;
  line-height: 1.5;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

.app-shell {
  min-height: 100vh;
  background: linear-gradient(180deg, #1e1e1e 0%, #141414 100%);
  color: #d0d0d0;
}

.app-header {
  position: sticky;
  top: 0;
  z-index: 10;
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  align-items: center;
  height: 40px;
  padding: 0 12px;
  background: linear-gradient(180deg, #252525 0%, #1e1e1e 100%);
  border-bottom: 1px solid #2a2a2a;
}

.app-header__left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.app-header__logo {
  display: flex;
  align-items: center;
  gap: 6px;
}

.app-header__logo-text {
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0.12em;
  color: #00b4d8;
}

.app-header__divider {
  width: 1px;
  height: 18px;
  background: #333;
}

.app-header__project-btn {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 8px;
  background: transparent;
  border: none;
  border-radius: 3px;
  color: #ccc;
  font-size: 13px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.12s ease;
  max-width: 200px;
}

.app-header__project-btn:hover {
  background: rgba(255, 255, 255, 0.06);
  color: #fff;
}

.app-header__project-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex: 1;
  text-align: left;
}

.app-header__project-arrow {
  font-size: 10px;
  color: #555;
  transition: transform 0.15s ease;
}

.app-header__project-btn:hover .app-header__project-arrow {
  color: #888;
}

.app-header__center {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
}

.app-header__action {
  width: 28px;
  height: 28px;
  padding: 0;
  border-radius: 3px;
}

.app-header__action :deep(.p-button-icon) {
  font-size: 14px;
}

.app-header__right {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
}

/* Classic toggle switch for header */
.app-header__toggle {
  display: flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
  user-select: none;
}

.app-header__toggle input {
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.app-header__toggle-track {
  position: relative;
  width: 28px;
  height: 16px;
  background: #333;
  border-radius: 8px;
  transition: all 0.2s ease;
  border: 1px solid #444;
}

.app-header__toggle-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 10px;
  height: 10px;
  background: #666;
  border-radius: 50%;
  transition: all 0.2s ease;
}

.app-header__toggle-label {
  font-size: 11px;
  color: #666;
  font-weight: 500;
  transition: color 0.2s ease;
}

.app-header__toggle:hover .app-header__toggle-track {
  border-color: #555;
}

.app-header__toggle:hover .app-header__toggle-thumb {
  background: #888;
}

.app-header__toggle:hover .app-header__toggle-label {
  color: #888;
}

/* Active state - red accent for Test Grid */
.app-header__toggle--active .app-header__toggle-track {
  background: rgba(255, 59, 48, 0.3);
  border-color: rgba(255, 59, 48, 0.5);
}

.app-header__toggle--active .app-header__toggle-thumb {
  left: 14px;
  background: #ff3b30;
}

.app-header__toggle--active .app-header__toggle-label {
  color: #ff3b30;
}

/* Disabled state */
.app-header__toggle--disabled {
  cursor: not-allowed;
  opacity: 0.4;
}

.app-header__toggle--disabled:hover .app-header__toggle-track {
  border-color: #444;
}

.app-header__toggle--disabled:hover .app-header__toggle-thumb {
  background: #666;
}

.app-header__toggle--disabled:hover .app-header__toggle-label {
  color: #666;
}

/* Connection status indicator */
.app-header__connection {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 4px 10px;
  background: rgba(0, 0, 0, 0.2);
  border: 1px solid #2a2a2a;
  border-radius: 3px;
}

.app-header__connection-led {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #555;
  box-shadow: 0 0 0 2px rgba(0, 0, 0, 0.3);
  transition: background 0.3s ease, box-shadow 0.3s ease;
}

.app-header__connection-label {
  font-size: 11px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: #666;
  transition: color 0.3s ease;
}

/* Connected state - green LED */
.app-header__connection--connected .app-header__connection-led {
  background: #4ade80;
  box-shadow:
    0 0 0 2px rgba(74, 222, 128, 0.2),
    0 0 8px rgba(74, 222, 128, 0.4);
}

.app-header__connection--connected .app-header__connection-label {
  color: #4ade80;
}

/* Checking state - orange LED with pulse animation */
.app-header__connection--checking .app-header__connection-led {
  background: #f59e0b;
  box-shadow:
    0 0 0 2px rgba(245, 158, 11, 0.2),
    0 0 8px rgba(245, 158, 11, 0.4);
  animation: pulse-led 1s ease-in-out infinite;
}

.app-header__connection--checking .app-header__connection-label {
  color: #f59e0b;
}

/* Offline state - red LED */
.app-header__connection--offline .app-header__connection-led {
  background: #ef4444;
  box-shadow:
    0 0 0 2px rgba(239, 68, 68, 0.2),
    0 0 8px rgba(239, 68, 68, 0.3);
}

.app-header__connection--offline .app-header__connection-label {
  color: #ef4444;
}

@keyframes pulse-led {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.5;
  }
}

/* Project menu dropdown styles */
.app-header__project-menu :deep(.p-menu) {
  background: #222;
  border: 1px solid #333;
  border-radius: 3px;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
  min-width: 200px;
  padding: 4px 0;
}

.app-header__project-menu :deep(.p-menu-list) {
  padding: 0;
}

.app-header__project-menu :deep(.p-menuitem-content) {
  padding: 0;
}

.app-header__project-menu :deep(.p-menuitem-link) {
  padding: 8px 12px;
  color: #aaa;
  font-size: 13px;
  transition: all 0.1s ease;
  border-radius: 0;
}

.app-header__project-menu :deep(.p-menuitem-link:hover) {
  background: rgba(0, 180, 216, 0.1);
  color: #ddd;
}

.app-header__project-menu :deep(.p-menuitem-icon) {
  color: #555;
  margin-right: 10px;
  font-size: 12px;
}

.app-header__project-menu :deep(.p-menuitem-link:hover .p-menuitem-icon) {
  color: #00b4d8;
}

.app-header__project-menu :deep(.p-menu-separator) {
  border-top: 1px solid #333;
  margin: 4px 0;
}

/* Welcome screen - shown when no project is open */
.app-welcome {
  display: flex;
  align-items: center;
  justify-content: center;
  height: calc(100vh - 41px);
  background: linear-gradient(180deg, #1a1a1a 0%, #111 100%);
}

.app-welcome__content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 24px;
  max-width: 400px;
  text-align: center;
}

.app-welcome__logo {
  font-size: 48px;
  font-weight: 800;
  letter-spacing: 0.15em;
  color: #00b4d8;
  text-shadow: 0 0 40px rgba(0, 180, 216, 0.3);
}

.app-welcome__tagline {
  font-size: 15px;
  color: #666;
  margin: -12px 0 0;
  letter-spacing: 0.04em;
}

.app-welcome__actions {
  display: flex;
  gap: 12px;
  margin-top: 8px;
}

.app-welcome__btn {
  padding: 10px 20px;
  font-size: 13px;
  font-weight: 500;
}

.app-welcome__btn--primary {
  background: #00b4d8;
  border-color: #00b4d8;
  color: #000;
}

.app-welcome__btn--primary:hover:not(:disabled) {
  background: #00c8f0;
  border-color: #00c8f0;
  color: #000;
}

.app-welcome__recent {
  margin-top: 24px;
  width: 100%;
}

.app-welcome__recent-title {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: #555;
  margin-bottom: 12px;
}

.app-welcome__recent-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.app-welcome__recent-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 14px;
  background: rgba(255, 255, 255, 0.02);
  border: 1px solid transparent;
  border-radius: 4px;
  color: #aaa;
  font-size: 13px;
  text-align: left;
  cursor: pointer;
  transition: all 0.12s ease;
}

.app-welcome__recent-item:hover {
  background: rgba(0, 180, 216, 0.08);
  border-color: rgba(0, 180, 216, 0.2);
  color: #ddd;
}

.app-welcome__recent-item i {
  color: #555;
  font-size: 14px;
}

.app-welcome__recent-item:hover i {
  color: #00b4d8;
}

.app-welcome__recent-name {
  flex: 1;
  font-weight: 500;
}

.app-welcome__recent-desc {
  font-size: 12px;
  color: #555;
  max-width: 150px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.app-main {
  height: calc(100vh - 41px);
}

/* Splitter component styling */
.app-main :deep(.p-splitter) {
  background: transparent;
  border: none;
  height: 100%;
}

.app-main :deep(.p-splitter-gutter) {
  background: #1a1a1a;
  transition: background 0.15s ease;
}

.app-main :deep(.p-splitter-gutter:hover),
.app-main :deep(.p-splitter-gutter-resizing) {
  background: #00b4d8;
}

.app-main :deep(.p-splitter-gutter-handle) {
  background: #3a3a3a;
  transition: background 0.15s ease;
}

.app-main :deep(.p-splitter-gutter:hover .p-splitter-gutter-handle),
.app-main :deep(.p-splitter-gutter-resizing .p-splitter-gutter-handle) {
  background: #00b4d8;
}

.app-panel {
  display: flex;
  flex-direction: column;
  background: #1e1e1e;
  padding: 0;
  overflow: hidden;
  height: 100%;
}

.app-panel--left {
  border-right: 1px solid #2a2a2a;
}

.app-panel--center {
  background: #181818;
}

.app-panel--right {
  border-left: 1px solid #2a2a2a;
}

.app-panel__header {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  font-weight: 600;
  color: #666;
  padding: 8px 12px;
  background: #1a1a1a;
  border-bottom: 1px solid #2a2a2a;
}

.app-panel__copy {
  color: #999;
  line-height: 1.6;
  font-size: 13px;
}

.app-panel--left :deep(.p-tabs) {
  height: 100%;
  display: flex;
  flex-direction: column;
}

.app-panel--left :deep(.p-tablist) {
  background: #1a1a1a;
  border-bottom: 1px solid #2a2a2a;
  gap: 0;
  padding: 0;
  flex-shrink: 0;
}

.app-panel--left :deep(.p-tab) {
  background: transparent;
  border: none;
  border-right: 1px solid #2a2a2a;
  color: #666;
  font-size: 11px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  padding: 6px 10px;
  border-radius: 0;
  transition: all 0.15s ease;
  position: relative;
}

.app-panel--left :deep(.p-tab:last-child) {
  border-right: none;
}

.app-panel--left :deep(.p-tab:hover) {
  color: #aaa;
  background: rgba(255, 255, 255, 0.03);
}

.app-panel--left :deep(.p-tab-active) {
  color: #00b4d8;
  background: transparent;
}

.app-panel--left :deep(.p-tab-active::after) {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 1px;
  background: #00b4d8;
}

.app-panel--left :deep(.p-tabpanels) {
  flex: 1;
  overflow-y: auto;
  overflow-x: hidden;
  padding: 10px;
  min-height: 0;
}

@media (max-width: 1024px) {
  .app-main {
    grid-template-columns: 1fr;
    height: auto;
  }

  .app-panel {
    min-height: 280px;
    border-right: none;
    border-bottom: 1px solid #2a2a2a;
  }
}

.project-dialog {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-width: min(360px, 90vw);
}

.project-dialog__label {
  font-size: 11px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: #666;
  margin-bottom: 4px;
  display: block;
}

.project-dialog__hint {
  font-size: 12px;
  color: #666;
  margin: 0 0 8px;
  font-style: italic;
}

.project-dialog__actions {
  display: flex;
  gap: 8px;
  justify-content: flex-end;
  margin-top: 8px;
  padding-top: 12px;
  border-top: 1px solid #2a2a2a;
}

/* Global PrimeVue overrides for MadMapper style */
:deep(.p-button) {
  background: #333;
  border: 1px solid #3a3a3a;
  color: #bbb;
  font-size: 12px;
  font-weight: 500;
  padding: 5px 12px;
  border-radius: 2px;
  transition: all 0.12s ease;
  box-shadow: none;
}

:deep(.p-button:hover:not(:disabled)) {
  background: #3a3a3a;
  border-color: #4a4a4a;
  color: #ddd;
}

:deep(.p-button:active:not(:disabled)) {
  background: #2a2a2a;
}

:deep(.p-button:focus-visible) {
  outline: none;
  box-shadow: 0 0 0 1px rgba(0, 180, 216, 0.5);
}

:deep(.p-button:disabled) {
  opacity: 0.35;
  cursor: not-allowed;
}

:deep(.p-button.p-button-text) {
  background: transparent;
  border: 1px solid transparent;
  color: #888;
  box-shadow: none;
  padding: 4px 8px;
}

:deep(.p-button.p-button-text:hover:not(:disabled)) {
  background: rgba(255, 255, 255, 0.05);
  border-color: transparent;
  color: #bbb;
}

:deep(.p-button.p-button-sm) {
  font-size: 11px;
  padding: 4px 10px;
}

:deep(.p-button .p-button-icon) {
  font-size: 12px;
  margin-right: 0;
}

:deep(.p-button .p-button-label) {
  font-weight: 500;
}

:deep(.p-button) {
  gap: 6px;
}

:deep(.p-button-icon-only) {
  gap: 0;
  padding: 5px 8px;
}

:deep(.p-button-icon-only .p-button-icon) {
  margin: 0;
}

:deep(.p-button.p-button-danger) {
  background: #8b3a3a;
  border-color: #7a3333;
}

:deep(.p-button.p-button-danger:hover:not(:disabled)) {
  background: #9e4444;
  border-color: #8b3a3a;
}

:deep(.p-button.p-button-danger.p-button-text) {
  background: transparent;
  color: #cc6666;
  border-color: transparent;
}

:deep(.p-button.p-button-danger.p-button-text:hover:not(:disabled)) {
  background: rgba(180, 60, 60, 0.12);
  color: #dd7777;
}

:deep(.p-inputtext) {
  background: #1a1a1a;
  border: 1px solid #333;
  color: #ccc;
  font-size: 11px;
  padding: 4px 6px;
  border-radius: 2px;
  transition: all 0.12s ease;
  line-height: 1.1;
  height: 24px;
}

:deep(.p-inputtext::placeholder) {
  color: #555;
}

:deep(.p-inputtext:hover:not(:focus)) {
  border-color: #3a3a3a;
}

:deep(.p-inputtext:focus) {
  border-color: #00b4d8;
  box-shadow: none;
  outline: none;
}

:deep(.p-textarea) {
  background: #1a1a1a;
  border: 1px solid #333;
  color: #ccc;
  font-size: 12px;
  padding: 6px 10px;
  border-radius: 2px;
  transition: all 0.12s ease;
  resize: vertical;
  min-height: 50px;
}

:deep(.p-textarea:focus) {
  border-color: #00b4d8;
  box-shadow: none;
  outline: none;
}

:deep(.p-dropdown) {
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 2px;
  transition: all 0.12s ease;
  min-height: 0;
  height: 24px;
}

:deep(.p-dropdown .p-dropdown-label) {
  font-size: 11px;
  color: #ccc;
  padding: 4px 6px;
  line-height: 1.1;
}

:deep(.p-dropdown .p-dropdown-label.p-placeholder) {
  color: #555;
}

:deep(.p-dropdown .p-dropdown-trigger) {
  width: 20px;
  padding: 0;
  color: #666;
}

:deep(.p-dropdown .p-dropdown-trigger .p-icon) {
  width: 8px;
  height: 8px;
}

:deep(.p-dropdown:hover:not(.p-focus)) {
  border-color: #3a3a3a;
}

:deep(.p-dropdown:hover:not(.p-focus) .p-dropdown-trigger) {
  color: #888;
}

:deep(.p-dropdown:focus),
:deep(.p-dropdown.p-focus) {
  border-color: #00b4d8;
  box-shadow: none;
}

:deep(.p-dropdown-panel) {
  background: #222;
  border: 1px solid #333;
  border-radius: 2px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.5);
  margin-top: 1px;
}

:deep(.p-dropdown-items-wrapper) {
  max-height: 200px;
}

:deep(.p-dropdown-item) {
  color: #aaa;
  padding: 4px 8px;
  font-size: 11px;
  transition: background 0.1s ease;
}

:deep(.p-dropdown-item:hover) {
  background: rgba(255, 255, 255, 0.06);
  color: #ddd;
}

:deep(.p-dropdown-item.p-highlight) {
  background: rgba(0, 180, 216, 0.15);
  color: #00b4d8;
}

:deep(.p-dropdown-empty-message) {
  color: #555;
  font-size: 11px;
  padding: 6px 8px;
  font-style: italic;
}

/* PrimeVue 4 Select component (replaces Dropdown) */
:deep(.p-select) {
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 2px;
  transition: all 0.12s ease;
  min-height: 0;
  height: 24px;
}

:deep(.p-select .p-select-label) {
  font-size: 11px;
  color: #ccc;
  padding: 4px 6px;
  line-height: 1.1;
}

:deep(.p-select .p-select-label.p-placeholder) {
  color: #555;
}

:deep(.p-select .p-select-dropdown) {
  width: 20px;
  padding: 0;
  color: #666;
}

:deep(.p-select .p-select-dropdown .p-icon) {
  width: 8px;
  height: 8px;
}

:deep(.p-select:hover:not(.p-focus)) {
  border-color: #3a3a3a;
}

:deep(.p-select:hover:not(.p-focus) .p-select-dropdown) {
  color: #888;
}

:deep(.p-select:focus),
:deep(.p-select.p-focus) {
  border-color: #00b4d8;
  box-shadow: none;
}

:deep(.p-select-overlay) {
  background: #222;
  border: 1px solid #333;
  border-radius: 2px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.5);
  margin-top: 1px;
}

:deep(.p-select-list-container) {
  max-height: 200px;
}

:deep(.p-select-option) {
  color: #aaa;
  padding: 4px 8px;
  font-size: 11px;
  transition: background 0.1s ease;
}

:deep(.p-select-option:hover) {
  background: rgba(255, 255, 255, 0.06);
  color: #ddd;
}

:deep(.p-select-option.p-highlight) {
  background: rgba(0, 180, 216, 0.15);
  color: #00b4d8;
}

:deep(.p-select-empty-message) {
  color: #555;
  font-size: 11px;
  padding: 6px 8px;
  font-style: italic;
}

:deep(.p-inputnumber) {
  width: 100%;
}

:deep(.p-inputnumber-input) {
  background: #1a1a1a;
  border: 1px solid #333;
  color: #ccc;
  font-size: 11px;
  padding: 4px 6px;
  border-radius: 2px;
  transition: all 0.12s ease;
  line-height: 1.1;
  height: 24px;
}

:deep(.p-inputnumber-input:focus) {
  border-color: #00b4d8;
  box-shadow: none;
  outline: none;
}

:deep(.p-inputnumber-button) {
  background: #252525;
  border-color: #333;
  color: #666;
  width: 20px;
}

:deep(.p-inputnumber-button:hover) {
  background: #2a2a2a;
  color: #888;
}

:deep(.p-inputnumber-button .p-icon) {
  width: 10px;
  height: 10px;
}

:deep(.p-datatable) {
  font-size: 12px;
  border-radius: 0;
  overflow: hidden;
}

:deep(.p-datatable .p-datatable-thead > tr > th) {
  background: #1a1a1a;
  border-color: #2a2a2a;
  color: #666;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  font-size: 11px;
  padding: 7px 12px;
}

:deep(.p-datatable .p-datatable-tbody > tr) {
  background: transparent;
  border-color: #2a2a2a;
  transition: background 0.1s ease;
  cursor: pointer;
}

:deep(.p-datatable .p-datatable-tbody > tr:hover) {
  background: rgba(255, 255, 255, 0.03);
}

:deep(.p-datatable .p-datatable-tbody > tr.p-highlight) {
  background: rgba(0, 180, 216, 0.1);
  color: #ddd;
}

:deep(.p-datatable .p-datatable-tbody > tr.p-highlight:hover) {
  background: rgba(0, 180, 216, 0.14);
}

:deep(.p-datatable .p-datatable-tbody > tr > td) {
  border-color: #2a2a2a;
  padding: 6px 12px;
  color: #aaa;
}

:deep(.p-datatable-emptymessage td) {
  color: #555;
  text-align: center;
  padding: 16px 10px;
  font-style: italic;
}

:deep(.p-dialog) {
  background: #222;
  border: 1px solid #333;
  border-radius: 2px;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.6);
  overflow: hidden;
}

:deep(.p-dialog .p-dialog-header) {
  background: #1a1a1a;
  border-bottom: 1px solid #2a2a2a;
  color: #bbb;
  padding: 10px 14px;
  font-weight: 500;
  font-size: 13px;
}

:deep(.p-dialog .p-dialog-header-icons) {
  gap: 2px;
}

:deep(.p-dialog .p-dialog-header-icon) {
  color: #666;
  width: 22px;
  height: 22px;
  border-radius: 2px;
  transition: all 0.12s ease;
}

:deep(.p-dialog .p-dialog-header-icon:hover) {
  background: rgba(255, 255, 255, 0.08);
  color: #aaa;
}

:deep(.p-dialog .p-dialog-content) {
  background: #222;
  padding: 14px;
}

/* Message component styling */
:deep(.p-message) {
  border-radius: 2px;
  margin: 0;
  padding: 6px 10px;
}

:deep(.p-message.p-message-error) {
  background: rgba(180, 60, 60, 0.12);
  border: 1px solid rgba(180, 60, 60, 0.25);
  color: #cc7777;
}

:deep(.p-message.p-message-warn) {
  background: rgba(180, 140, 40, 0.12);
  border: 1px solid rgba(180, 140, 40, 0.25);
  color: #ccaa55;
}

:deep(.p-message .p-message-text) {
  font-size: 12px;
}

/* Toggle button styling */
:deep(.p-togglebutton) {
  background: #2a2a2a;
  border: 1px solid #333;
  color: #777;
  font-size: 11px;
  font-weight: 500;
  padding: 5px 10px;
  border-radius: 2px;
  transition: all 0.12s ease;
}

:deep(.p-togglebutton:hover) {
  background: #2e2e2e;
  border-color: #3a3a3a;
}

:deep(.p-togglebutton.p-togglebutton-checked) {
  background: rgba(0, 180, 216, 0.1);
  border-color: rgba(0, 180, 216, 0.4);
  color: #00b4d8;
}

/* FileUpload styling */
:deep(.p-fileupload-choose) {
  background: #333;
  border: 1px solid #3a3a3a;
  color: #bbb;
  font-size: 11px;
  font-weight: 500;
  padding: 4px 10px;
  border-radius: 2px;
  transition: all 0.12s ease;
}

:deep(.p-fileupload-choose:hover) {
  background: #3a3a3a;
  border-color: #4a4a4a;
}

</style>
