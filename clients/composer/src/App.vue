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

const menu = ref<InstanceType<typeof Menu> | null>(null);
const activeBrowserTab = ref("assets");
const showNewProjectDialog = ref(false);
const newProjectName = ref("");
const newProjectDescription = ref("");
const isCreatingProject = ref(false);
const newProjectError = ref<string | null>(null);

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
    id: `project-${createProjectId()}`,
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
    const response = await fetch("/api/projects", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    if (!response.ok) {
      const message = await response.text();
      throw new Error(message || "Failed to create project.");
    }

    showNewProjectDialog.value = false;
  } catch (error) {
    newProjectError.value =
      error instanceof Error ? error.message : "Failed to create project.";
  } finally {
    isCreatingProject.value = false;
  }
};

const createProjectId = () => {
  if (typeof crypto !== "undefined" && "randomUUID" in crypto) {
    return crypto.randomUUID();
  }
  return `${Date.now()}-${Math.random().toString(16).slice(2)}`;
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
        <div class="app-panel__header">Edit Area</div>
        <p class="app-panel__copy">
          Editing canvas for the selected browser item goes here.
        </p>
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
</style>
