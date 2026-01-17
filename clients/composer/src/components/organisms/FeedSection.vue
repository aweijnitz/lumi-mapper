<script setup lang="ts">
import { computed, ref } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import InputText from "primevue/inputtext";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useFeedStore } from "../../stores/feedStore";
import { useAssetStore } from "../../stores/assetStore";
import { createId } from "../../composables/useIds";
import type { Feed } from "../../types/feed";

const projectStore = useProjectStore();
const feedStore = useFeedStore();
const assetStore = useAssetStore();
const { feeds, activeFeed, isLoading, error } = storeToRefs(feedStore);
const { activeAsset } = storeToRefs(assetStore);

const name = ref("");

const selectedAssetPath = computed(() => activeAsset.value?.path ?? "");

const canCreate = computed(() =>
  Boolean(
    projectStore.activeProject &&
      name.value.trim().length > 0 &&
      selectedAssetPath.value &&
      !isLoading.value,
  ),
);

const createFeed = async () => {
  if (!projectStore.activeProject) {
    return;
  }

  const payload: Feed = {
    projectId: projectStore.activeProject.id,
    id: createId("feed"),
    name: name.value.trim(),
    type: "VideoFile",
    configJson: {
      filePath: selectedAssetPath.value,
    },
  };

  await feedStore.createFeed(payload);
  name.value = "";
};

</script>

<template>
  <section class="feed-section">
    <SectionHeader title="Feeds" subtitle="Create feeds and bind them to assets." />

    <div v-if="!projectStore.activeProject" class="feed-section__empty">
      Select a project to manage feeds.
    </div>

    <template v-else>
      <div class="feed-section__form">
        <InputText v-model="name" placeholder="Feed name" />
        <InputText :model-value="selectedAssetPath" placeholder="Select an asset" readonly />
        <Button label="Add Feed" icon="pi pi-plus" :disabled="!canCreate" @click="createFeed" />
      </div>

      <Message v-if="error" severity="error" class="feed-section__message">
        {{ error }}
      </Message>

      <DataTable
        :value="feeds"
        selectionMode="single"
        dataKey="id"
        v-model:selection="activeFeed"
        responsiveLayout="scroll"
        size="small"
      >
        <Column field="name" header="Feed" />
        <Column field="type" header="Type" />
      </DataTable>
    </template>
  </section>
</template>

<style scoped>
.feed-section {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.feed-section__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) auto;
  gap: 8px;
  align-items: center;
}

.feed-section__empty {
  color: #4a4640;
}
</style>
