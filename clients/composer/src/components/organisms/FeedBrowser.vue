<script setup lang="ts">
import { computed, ref, watch } from "vue";
import { storeToRefs } from "pinia";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import Dropdown from "primevue/dropdown";
import InputText from "primevue/inputtext";
import Button from "primevue/button";
import Message from "primevue/message";
import { useProjectStore } from "../../stores/projectStore";
import { useFeedStore } from "../../stores/feedStore";
import { useAssetStore } from "../../stores/assetStore";
import { createId } from "../../composables/useIds";
import type { Feed } from "../../types/feed";

const projectStore = useProjectStore();
const feedStore = useFeedStore();
const assetStore = useAssetStore();
const { feeds, activeFeed, error } = storeToRefs(feedStore);

const name = ref("");
const selectedAssetPath = ref("");

const assetOptions = computed(() =>
  assetStore.assets.map((asset) => ({ label: asset.name, value: asset.path })),
);

const hasActiveProject = computed(() => Boolean(projectStore.activeProject));
const isBusy = computed(() => feedStore.isLoading);
const canCreate = computed(
  () =>
    hasActiveProject.value &&
    name.value.trim().length > 0 &&
    selectedAssetPath.value.trim().length > 0 &&
    !isBusy.value,
);
const canUpdate = computed(() => Boolean(activeFeed.value && canCreate.value));

const resolveFeedFilePath = (feed: Feed | null) => {
  if (!feed) {
    return "";
  }
  const config = feed.configJson;
  if (typeof config === "string") {
    try {
      const parsed = JSON.parse(config) as { filePath?: string };
      return parsed.filePath ?? "";
    } catch {
      return "";
    }
  }
  if (config && typeof config === "object" && "filePath" in config) {
    return String((config as { filePath?: string }).filePath ?? "");
  }
  return "";
};

const syncFormWithFeed = (feed: Feed | null) => {
  if (!feed) {
    name.value = "";
    selectedAssetPath.value = "";
    return;
  }
  name.value = feed.name;
  selectedAssetPath.value = resolveFeedFilePath(feed);
};

watch(
  () => activeFeed.value?.id,
  () => {
    syncFormWithFeed(activeFeed.value);
  },
  { immediate: true },
);

const clearSelection = () => {
  feedStore.setActiveFeed(null);
  syncFormWithFeed(null);
};

const buildPayload = (feedId: string): Feed | null => {
  const projectId = projectStore.activeProject?.id;
  if (!projectId) {
    return null;
  }

  return {
    projectId,
    id: feedId,
    name: name.value.trim(),
    type: "VideoFile",
    configJson: { filePath: selectedAssetPath.value.trim() },
  };
};

const createFeed = async () => {
  const payload = buildPayload(createId("feed"));
  if (!payload) {
    return;
  }
  await feedStore.createFeed(payload);
};

const updateFeed = async () => {
  if (!activeFeed.value) {
    return;
  }
  const payload = buildPayload(activeFeed.value.id);
  if (!payload) {
    return;
  }
  payload.type = activeFeed.value.type;
  await feedStore.updateFeed(payload);
};

const deleteFeed = async () => {
  if (!activeFeed.value || !projectStore.activeProject) {
    return;
  }
  await feedStore.deleteFeed(projectStore.activeProject.id, activeFeed.value.id);
  clearSelection();
};
</script>

<template>
  <div class="feed-browser">
    <div v-if="!hasActiveProject" class="feed-browser__empty">
      Select a project to manage feeds.
    </div>

    <template v-else>
      <div class="feed-browser__form">
        <InputText
          v-model="name"
          placeholder="Feed name"
          data-testid="feed-name"
        />
        <Dropdown
          v-model="selectedAssetPath"
          :options="assetOptions"
          optionLabel="label"
          optionValue="value"
          placeholder="Select asset"
          appendTo="self"
          data-testid="feed-asset"
        />
      </div>

      <div class="feed-browser__actions">
        <Button
          label="Create Feed"
          icon="pi pi-plus"
          :disabled="!canCreate"
          :loading="isBusy"
          data-testid="feed-create"
          @click="createFeed"
        />
        <Button
          label="Update"
          icon="pi pi-save"
          :disabled="!canUpdate"
          :loading="isBusy"
          data-testid="feed-update"
          @click="updateFeed"
        />
        <Button
          label="Clear"
          text
          :disabled="isBusy"
          data-testid="feed-clear"
          @click="clearSelection"
        />
        <Button
          label="Delete"
          severity="danger"
          text
          :disabled="!activeFeed || isBusy"
          data-testid="feed-delete"
          @click="deleteFeed"
        />
      </div>

      <Message v-if="error" severity="error" class="feed-browser__message">
        {{ error }}
      </Message>

      <DataTable
        :value="feeds"
        selectionMode="single"
        dataKey="id"
        v-model:selection="activeFeed"
        responsiveLayout="scroll"
        class="feed-browser__table"
        size="small"
      >
        <Column field="name" header="Feed" />
        <Column field="type" header="Type" />
      </DataTable>
    </template>
  </div>
</template>

<style scoped>
.feed-browser {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.feed-browser__form {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 8px;
}

.feed-browser__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.feed-browser__empty {
  color: #4a4640;
}

.feed-browser__message {
  margin: 0;
}
</style>
