<script setup lang="ts">
import { computed, ref, watch } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import InputText from "primevue/inputtext";
import Textarea from "primevue/textarea";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useSceneStore } from "../../stores/sceneStore";
import { useCueStore } from "../../stores/cueStore";
import { createId } from "../../composables/useIds";
import type { Scene } from "../../types/scene";

const projectStore = useProjectStore();
const sceneStore = useSceneStore();
const cueStore = useCueStore();
const { scenes, activeScene, isLoading, error } = storeToRefs(sceneStore);

const name = ref("");
const description = ref("");
const localError = ref<string | null>(null);

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
    return;
  }
  name.value = scene.name;
  description.value = scene.description;
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

  const payload: Scene = {
    projectId: projectStore.activeProject.id,
    id: createId("scene"),
    name: name.value.trim(),
    description: description.value.trim(),
    surfaces: [],
  };

  await sceneStore.createScene(payload);
  name.value = "";
  description.value = "";
};

const updateScene = async () => {
  if (!projectStore.activeProject || !activeScene.value) {
    return;
  }
  const payload: Scene = {
    ...activeScene.value,
    name: name.value.trim(),
    description: description.value.trim(),
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
  gap: 12px;
}

.scene-section__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 8px;
  align-items: center;
}

.scene-section__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.scene-section__empty {
  color: #4a4640;
}
</style>
