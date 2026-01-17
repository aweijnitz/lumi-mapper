<script setup lang="ts">
import { computed, ref } from "vue";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import Dropdown from "primevue/dropdown";
import InputText from "primevue/inputtext";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";
import { createId } from "../../composables/useIds";
import type { Surface } from "../../types/surface";

const sceneStore = useSceneStore();
const feedStore = useFeedStore();

const name = ref("");
const selectedFeedId = ref<string | null>(null);

const sceneOptions = computed(() =>
  sceneStore.scenes.map((scene) => ({ label: scene.name, value: scene.id })),
);

const selectedSceneId = computed({
  get: () => sceneStore.activeScene?.id ?? null,
  set: (value: string | null) => {
    if (!value) {
      sceneStore.setActiveScene(null);
      return;
    }
    const match = sceneStore.scenes.find((scene) => scene.id === value) ?? null;
    sceneStore.setActiveScene(match);
  },
});

const feedOptions = computed(() =>
  feedStore.feeds.map((feed) => ({ label: feed.name, value: feed.id })),
);

const canCreate = computed(() =>
  Boolean(sceneStore.activeScene && name.value.trim().length > 0 && selectedFeedId.value),
);

const createSurface = async () => {
  if (!sceneStore.activeScene || !selectedFeedId.value) {
    return;
  }

  const newSurface: Surface = {
    id: createId("surface"),
    name: name.value.trim(),
    vertices: [
      { x: -0.5, y: -0.4 },
      { x: 0.5, y: -0.4 },
      { x: 0.5, y: 0.4 },
      { x: -0.5, y: 0.4 },
    ],
    feedId: selectedFeedId.value,
    opacity: 1,
    brightness: 1,
    blendMode: "Normal",
    zOrder: 0,
  };

  const nextScene = {
    ...sceneStore.activeScene,
    surfaces: [...sceneStore.activeScene.surfaces, newSurface],
  };

  await sceneStore.updateScene(nextScene);
  name.value = "";
  selectedFeedId.value = null;
};
</script>

<template>
  <section class="surface-section">
    <SectionHeader title="Surfaces" subtitle="Attach feeds to surfaces in the active scene." />
    <div class="surface-section__form surface-section__form--scene">
      <Dropdown
        v-model="selectedSceneId"
        :options="sceneOptions"
        optionLabel="label"
        optionValue="value"
        placeholder="Select a scene"
        aria-label="Select a scene"
        appendTo="self"
      />
    </div>

    <div v-if="!sceneStore.activeScene" class="surface-section__empty">
      Select a scene to add surfaces.
    </div>

    <template v-else>
      <div class="surface-section__form">
        <InputText v-model="name" placeholder="Surface name" />
        <Dropdown
          v-model="selectedFeedId"
          :options="feedOptions"
          optionLabel="label"
          optionValue="value"
          placeholder="Select a feed"
          aria-label="Select a feed"
          appendTo="self"
        />
        <Button label="Add Surface" icon="pi pi-plus" :disabled="!canCreate" @click="createSurface" />
      </div>

      <Message v-if="sceneStore.error" severity="error" class="surface-section__message">
        {{ sceneStore.error }}
      </Message>

      <DataTable
        :value="sceneStore.activeScene.surfaces"
        dataKey="id"
        responsiveLayout="scroll"
        size="small"
      >
        <Column field="name" header="Surface" />
        <Column field="feedId" header="Feed" />
        <Column field="blendMode" header="Blend" />
      </DataTable>
    </template>
  </section>
</template>

<style scoped>
.surface-section {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.surface-section__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) auto;
  gap: 8px;
  align-items: center;
}

.surface-section__form--scene {
  grid-template-columns: minmax(0, 1fr);
}

.surface-section__empty {
  color: #4a4640;
}
</style>
