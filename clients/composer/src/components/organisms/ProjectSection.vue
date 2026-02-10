<script setup lang="ts">
import { computed, ref } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import Dialog from "primevue/dialog";
import InputText from "primevue/inputtext";
import Textarea from "primevue/textarea";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { createId } from "../../composables/useIds";
import type { Project } from "../../types/project";

const projectStore = useProjectStore();
const { projects, activeProject, isLoading, error } = storeToRefs(projectStore);
const emit = defineEmits<{ (event: "save", project: Project): void }>();

const showDialog = ref(false);
const name = ref("");
const description = ref("");

const nameError = computed(() => {
  const trimmed = name.value.trim();
  if (!trimmed) {
    return "Project name is required.";
  }
  if (trimmed.length > 64) {
    return "Project name must be 64 characters or less.";
  }
  return null;
});

const descriptionError = computed(() => {
  const trimmed = description.value.trim();
  if (trimmed.length > 280) {
    return "Project description must be 280 characters or less.";
  }
  return null;
});

const canCreate = computed(() => !nameError.value && !descriptionError.value && !isLoading.value);

const openDialog = () => {
  name.value = "";
  description.value = "";
  showDialog.value = true;
};

const createProject = async () => {
  if (!canCreate.value) {
    return;
  }

  const payload: Project = {
    id: createId("project"),
    name: name.value.trim(),
    description: description.value.trim(),
    createdAt: new Date().toISOString(),
    updatedAt: new Date().toISOString(),
    assetIds: [],
    sceneIds: [],
    feedIds: [],
    cueOrder: [],
    settings: {
      controllers: {},
      midiChannels: [],
      globalConfig: {},
    },
  };

  await projectStore.createProject(payload);
  showDialog.value = false;
};

const saveProject = () => {
  if (activeProject.value) {
    emit("save", activeProject.value);
  }
};
</script>

<template>
  <section class="project-section">
    <SectionHeader title="Projects" subtitle="Create or select a project." />

    <div class="project-section__actions">
      <Button label="New Project" icon="pi pi-plus" @click="openDialog" />
      <Button
        label="Save Project"
        icon="pi pi-save"
        text
        :disabled="!projectStore.activeProject"
        @click="saveProject"
      />
    </div>

    <Message v-if="error" severity="error" class="project-section__message">
      {{ error }}
    </Message>

    <DataTable
      :value="projects"
      selectionMode="single"
      dataKey="id"
      v-model:selection="activeProject"
      responsiveLayout="scroll"
      size="small"
    >
      <Column field="name" header="Name" />
      <Column field="description" header="Description" />
    </DataTable>

    <Dialog v-model:visible="showDialog" modal header="New Project">
      <div class="project-section__form">
        <label class="project-section__label" for="project-name">Project name</label>
        <InputText id="project-name" v-model="name" autocomplete="off" />
        <Message v-if="nameError" severity="error" class="project-section__field-error">
          {{ nameError }}
        </Message>

        <label class="project-section__label" for="project-description">Project description</label>
        <Textarea
          id="project-description"
          v-model="description"
          rows="4"
          autoResize
          autocomplete="off"
        />
        <Message v-if="descriptionError" severity="error" class="project-section__field-error">
          {{ descriptionError }}
        </Message>
      </div>

      <template #footer>
        <Button label="Cancel" text @click="showDialog = false" />
        <Button label="Create" :disabled="!canCreate" :loading="isLoading" @click="createProject" />
      </template>
    </Dialog>
  </section>
</template>

<style scoped>
.project-section {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.project-section__actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.project-section__form {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.project-section__label {
  font-weight: 600;
  font-size: 0.85rem;
}

.project-section__field-error {
  margin: 0;
}
</style>
