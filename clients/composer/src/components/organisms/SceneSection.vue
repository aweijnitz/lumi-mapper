<script setup lang="ts">
import { computed, ref, watch } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import InputText from "primevue/inputtext";
import Textarea from "primevue/textarea";
import Message from "primevue/message";
import Select from "primevue/select";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useSceneStore } from "../../stores/sceneStore";
import { useCueStore } from "../../stores/cueStore";
import { createId } from "../../composables/useIds";
import type { Scene, SceneFilter, SceneSettings } from "../../types/scene";
import { defaultSceneSettings, sceneFilterLabels } from "../../types/scene";

const projectStore = useProjectStore();
const sceneStore = useSceneStore();
const cueStore = useCueStore();
const { scenes, activeScene, isLoading, error } = storeToRefs(sceneStore);

const name = ref("");
const description = ref("");
const filter = ref<SceneFilter>("none");
const colorPaletteIndex = ref(0);
const localError = ref<string | null>(null);

// Filter options for the dropdown
const filterOptions = Object.entries(sceneFilterLabels).map(([value, label]) => ({
  value: value as SceneFilter,
  label,
}));

// Palette options (matches renderer's kNumPalettes = 5)
const paletteOptions = [
  { value: 0, label: "Mixed Neon" },
  { value: 1, label: "Cyan/Magenta" },
  { value: 2, label: "Fire/Ice" },
  { value: 3, label: "Tropical" },
  { value: 4, label: "Noir" },
];

const canCreate = computed(() =>
  Boolean(projectStore.activeProject && name.value.trim().length > 0 && !isLoading.value),
);

const canUpdate = computed(() =>
  Boolean(activeScene.value && name.value.trim().length > 0 && !isLoading.value),
);

const isSceneReferenced = computed(() => {
  if (!activeScene.value) {
    return false;
  }
  return cueStore.cues.some((cue) => cue.sceneId === activeScene.value?.id);
});

const deleteSceneTitle = computed(() => {
  if (!activeScene.value) {
    return "Select a scene to delete.";
  }
  if (isSceneReferenced.value) {
    return "Remove cues that reference this scene before deleting.";
  }
  return "Delete the selected scene.";
});

const syncForm = (scene: Scene | null) => {
  if (!scene) {
    name.value = "";
    description.value = "";
    filter.value = "none";
    colorPaletteIndex.value = 0;
    return;
  }
  name.value = scene.name;
  description.value = scene.description;
  const settings = scene.settings ?? defaultSceneSettings;
  filter.value = settings.filter;
  colorPaletteIndex.value = settings.colorPaletteIndex;
};

watch(
  () => activeScene.value?.id,
  () => {
    localError.value = null;
    syncForm(activeScene.value);
  },
  { immediate: true },
);

const createScene = async () => {
  if (!projectStore.activeProject) {
    return;
  }

  const settings: SceneSettings = {
    filter: filter.value,
    colorPaletteIndex: colorPaletteIndex.value,
  };

  const payload: Scene = {
    projectId: projectStore.activeProject.id,
    id: createId("scene"),
    name: name.value.trim(),
    description: description.value.trim(),
    surfaces: [],
    settings,
  };

  await sceneStore.createScene(payload);
  name.value = "";
  description.value = "";
  filter.value = "none";
  colorPaletteIndex.value = 0;
};

const updateScene = async () => {
  if (!projectStore.activeProject || !activeScene.value) {
    return;
  }

  const settings: SceneSettings = {
    filter: filter.value,
    colorPaletteIndex: colorPaletteIndex.value,
  };

  const payload: Scene = {
    ...activeScene.value,
    name: name.value.trim(),
    description: description.value.trim(),
    settings,
  };
  await sceneStore.updateScene(payload);
};

const deleteScene = async () => {
  localError.value = null;
  if (!projectStore.activeProject || !activeScene.value) {
    return;
  }
  if (isSceneReferenced.value) {
    localError.value = "Cannot delete a scene referenced by a cue.";
    return;
  }
  const confirmed = window.confirm(`Delete scene ${activeScene.value.name}?`);
  if (!confirmed) {
    return;
  }
  await sceneStore.deleteScene(projectStore.activeProject.id, activeScene.value.id);
  if (sceneStore.scenes.length > 0) {
    sceneStore.setActiveScene(sceneStore.scenes[0]);
  }
};
</script>

<template>
  <section class="scene-section">
    <SectionHeader title="Scenes" subtitle="Create scenes for the active project." />

    <div v-if="!projectStore.activeProject" class="scene-section__empty">
      Select a project to manage scenes.
    </div>

    <template v-else>
      <div class="scene-section__form">
        <InputText v-model="name" placeholder="Scene name" />
        <Textarea v-model="description" placeholder="Scene description" rows="2" autoResize />
      </div>
      <div class="scene-section__settings">
        <div class="scene-section__setting">
          <label class="scene-section__label">Filter</label>
          <Select
            v-model="filter"
            :options="filterOptions"
            optionLabel="label"
            optionValue="value"
            placeholder="Select filter"
            class="scene-section__select"
          />
        </div>
        <div v-if="filter === 'colorTint'" class="scene-section__setting">
          <label class="scene-section__label">Palette</label>
          <Select
            v-model="colorPaletteIndex"
            :options="paletteOptions"
            optionLabel="label"
            optionValue="value"
            placeholder="Select palette"
            class="scene-section__select"
          />
        </div>
      </div>
      <div class="scene-section__actions">
        <Button label="Add Scene" icon="pi pi-plus" :disabled="!canCreate" @click="createScene" />
        <Button label="Update" icon="pi pi-save" :disabled="!canUpdate" @click="updateScene" />
        <Button
          label="Delete"
          icon="pi pi-trash"
          severity="danger"
          text
          :disabled="!activeScene || isLoading || isSceneReferenced"
          :title="deleteSceneTitle"
          @click="deleteScene"
        />
      </div>

      <Message v-if="localError" severity="warn" class="scene-section__message">
        {{ localError }}
      </Message>

      <Message v-if="error" severity="error" class="scene-section__message">
        {{ error }}
      </Message>

      <DataTable
        :value="scenes"
        selectionMode="single"
        dataKey="id"
        v-model:selection="activeScene"
        responsiveLayout="scroll"
        size="small"
      >
        <Column field="name" header="Scene" />
        <Column field="description" header="Description" />
      </DataTable>
    </template>
  </section>
</template>

<style scoped>
.scene-section {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.scene-section__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 6px;
  align-items: start;
}

.scene-section__settings {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  padding: 8px 0;
}

.scene-section__setting {
  display: flex;
  align-items: center;
  gap: 8px;
}

.scene-section__label {
  font-size: 12px;
  color: #888;
  min-width: 50px;
}

.scene-section__select {
  min-width: 140px;
}

.scene-section__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  align-items: center;
  padding-top: 6px;
  border-top: 1px solid #2a2a2a;
}

.scene-section__empty {
  color: #555;
  font-size: 12px;
  padding: 12px 0;
  text-align: center;
  font-style: italic;
}

.scene-section__message {
  margin: 0;
}
</style>
