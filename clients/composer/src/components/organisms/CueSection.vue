<script setup lang="ts">
import { computed, ref, watch } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import Dropdown from "primevue/dropdown";
import InputText from "primevue/inputtext";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useSceneStore } from "../../stores/sceneStore";
import { useCueStore } from "../../stores/cueStore";
import { createId } from "../../composables/useIds";
import type { Cue } from "../../types/cue";

const projectStore = useProjectStore();
const sceneStore = useSceneStore();
const cueStore = useCueStore();
const { cues, activeCue, isLoading, error } = storeToRefs(cueStore);

const name = ref("");
const selectedSceneId = ref<string | null>(null);
const localError = ref<string | null>(null);

const sceneOptions = computed(() =>
  sceneStore.scenes.map((scene) => ({ label: scene.name, value: scene.id })),
);

const canCreate = computed(() =>
  Boolean(
    projectStore.activeProject &&
      name.value.trim().length > 0 &&
      selectedSceneId.value &&
      !isLoading.value,
  ),
);

const canUpdate = computed(() =>
  Boolean(
    projectStore.activeProject &&
      activeCue.value &&
      name.value.trim().length > 0 &&
      selectedSceneId.value &&
      !isLoading.value,
  ),
);

const syncForm = (cue: Cue | null) => {
  if (!cue) {
    name.value = "";
    selectedSceneId.value = null;
    return;
  }
  name.value = cue.name;
  selectedSceneId.value = cue.sceneId;
};

watch(
  () => activeCue.value?.id,
  () => {
    localError.value = null;
    syncForm(activeCue.value);
  },
  { immediate: true },
);

const createCue = async () => {
  if (!projectStore.activeProject || !selectedSceneId.value) {
    return;
  }

  const payload: Cue = {
    projectId: projectStore.activeProject.id,
    id: createId("cue"),
    name: name.value.trim(),
    sceneId: selectedSceneId.value,
    surfaceOpacities: [],
    surfaceBrightnesses: [],
  };

  const created = await cueStore.createCue(payload);
  if (projectStore.activeProject) {
    const nextProject = {
      ...projectStore.activeProject,
      cueOrder: [...projectStore.activeProject.cueOrder, created.id],
    };
    projectStore.setActiveProject(nextProject);
  }

  name.value = "";
  selectedSceneId.value = null;
};

const updateCue = async () => {
  if (!projectStore.activeProject || !activeCue.value || !selectedSceneId.value) {
    return;
  }
  const payload: Cue = {
    ...activeCue.value,
    name: name.value.trim(),
    sceneId: selectedSceneId.value,
  };
  await cueStore.updateCue(payload);
};

const deleteCue = async () => {
  localError.value = null;
  if (!projectStore.activeProject || !activeCue.value) {
    return;
  }
  const confirmed = window.confirm(`Delete cue ${activeCue.value.name}?`);
  if (!confirmed) {
    return;
  }
  const nextProject = {
    ...projectStore.activeProject,
    cueOrder: projectStore.activeProject.cueOrder.filter((id) => id !== activeCue.value?.id),
  };
  try {
    await projectStore.updateProject(nextProject);
    await cueStore.deleteCue(projectStore.activeProject.id, activeCue.value.id);
    if (cueStore.cues.length > 0) {
      cueStore.setActiveCue(cueStore.cues[0]);
    }
  } catch (err) {
    localError.value = "Failed to delete cue. Ensure it is not referenced by the project.";
  }
};
</script>

<template>
  <section class="cue-section">
    <SectionHeader title="Cues" subtitle="Create cues to drive playback." />

    <div v-if="!projectStore.activeProject" class="cue-section__empty">
      Select a project to manage cues.
    </div>

    <template v-else>
      <div class="cue-section__form">
        <InputText v-model="name" placeholder="Cue name" />
        <Dropdown
          v-model="selectedSceneId"
          :options="sceneOptions"
          optionLabel="label"
          optionValue="value"
          placeholder="Select scene"
          aria-label="Select scene"
          appendTo="self"
        />
      </div>
      <div class="cue-section__actions">
        <Button label="Add Cue" icon="pi pi-plus" :disabled="!canCreate" @click="createCue" />
        <Button label="Update" icon="pi pi-save" :disabled="!canUpdate" @click="updateCue" />
        <Button
          label="Delete"
          icon="pi pi-trash"
          severity="danger"
          text
          :disabled="!activeCue || isLoading"
          @click="deleteCue"
        />
      </div>

      <Message v-if="localError" severity="warn" class="cue-section__message">
        {{ localError }}
      </Message>

      <Message v-if="error" severity="error" class="cue-section__message">
        {{ error }}
      </Message>

      <DataTable
        :value="cues"
        selectionMode="single"
        dataKey="id"
        v-model:selection="activeCue"
        responsiveLayout="scroll"
        size="small"
      >
        <Column field="name" header="Cue" />
        <Column field="sceneId" header="Scene" />
      </DataTable>
    </template>
  </section>
</template>

<style scoped>
.cue-section {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.cue-section__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 6px;
  align-items: start;
}

.cue-section__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  align-items: center;
  padding-top: 6px;
  border-top: 1px solid #2a2a2a;
}

.cue-section__empty {
  color: #555;
  font-size: 12px;
  padding: 12px 0;
  text-align: center;
  font-style: italic;
}

.cue-section__message {
  margin: 0;
}
</style>
