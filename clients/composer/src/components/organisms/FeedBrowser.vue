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
const successMessage = ref("");

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

// Extract just the filename from a full path
const getFileName = (feed: Feed) => {
  const path = resolveFeedFilePath(feed);
  if (!path) return "—";
  const parts = path.split("/");
  return parts[parts.length - 1] || "—";
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

const showSuccess = (message: string) => {
  successMessage.value = message;
  setTimeout(() => {
    successMessage.value = "";
  }, 2000);
};

const createFeed = async () => {
  const payload = buildPayload(createId("feed"));
  if (!payload) {
    return;
  }
  await feedStore.createFeed(payload);
  if (!feedStore.error) {
    showSuccess("Feed created");
    clearSelection();
  }
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
  if (!feedStore.error) {
    showSuccess("Feed updated");
  }
};

const deleteFeed = async () => {
  if (!activeFeed.value || !projectStore.activeProject) {
    return;
  }
  await feedStore.deleteFeed(projectStore.activeProject.id, activeFeed.value.id);
  if (!feedStore.error) {
    showSuccess("Feed deleted");
  }
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
        <div v-if="activeFeed" class="feed-browser__editing">
          <i class="pi pi-pencil"></i>
          <span>Editing: <strong>{{ activeFeed.name }}</strong></span>
        </div>
        <div class="feed-browser__field">
          <label class="feed-browser__label" for="feed-name" title="Display name for this feed">Feed Name</label>
          <InputText
            id="feed-name"
            v-model="name"
            placeholder="e.g., Main Video, Background Loop"
            data-testid="feed-name"
            title="Enter a descriptive name for this video feed"
          />
        </div>
        <div class="feed-browser__field">
          <label class="feed-browser__label" for="feed-asset" title="Video or image file to use">Source Asset</label>
          <Dropdown
            id="feed-asset"
            v-model="selectedAssetPath"
            :options="assetOptions"
            optionLabel="label"
            optionValue="value"
            placeholder="Choose an uploaded asset"
            appendTo="self"
            data-testid="feed-asset"
            title="Select a video or image from your uploaded assets"
          />
        </div>
      </div>

      <div class="feed-browser__actions">
        <Button
          label="Create"
          icon="pi pi-plus"
          :disabled="!canCreate"
          :loading="isBusy"
          data-testid="feed-create"
          title="Create a new feed with the settings above"
          @click="createFeed"
        />
        <Button
          label="Update"
          icon="pi pi-save"
          :disabled="!canUpdate"
          :loading="isBusy"
          data-testid="feed-update"
          title="Save changes to the selected feed"
          @click="updateFeed"
        />
        <Button
          label="Clear"
          icon="pi pi-eraser"
          text
          :disabled="isBusy"
          data-testid="feed-clear"
          title="Clear the form and deselect"
          @click="clearSelection"
        />
        <Button
          label="Delete"
          icon="pi pi-trash"
          severity="danger"
          text
          :disabled="!activeFeed || isBusy"
          data-testid="feed-delete"
          title="Permanently delete the selected feed"
          @click="deleteFeed"
        />
      </div>

      <Message v-if="error" severity="error" class="feed-browser__message">
        {{ error }}
      </Message>

      <Transition name="feed-browser__success">
        <div v-if="successMessage" class="feed-browser__success">
          <i class="pi pi-check-circle"></i>
          {{ successMessage }}
        </div>
      </Transition>

      <DataTable
        :value="feeds"
        selectionMode="single"
        dataKey="id"
        v-model:selection="activeFeed"
        responsiveLayout="scroll"
        class="feed-browser__table"
        size="small"
      >
        <Column field="name" header="Feed">
          <template #body="{ data }">
            <span class="feed-browser__name" :class="{ 'feed-browser__name--editing': activeFeed?.id === data.id }">
              <i v-if="activeFeed?.id === data.id" class="pi pi-pencil feed-browser__edit-icon"></i>
              {{ data.name }}
            </span>
          </template>
        </Column>
        <Column header="Source">
          <template #body="{ data }">
            <span class="feed-browser__source" :title="resolveFeedFilePath(data)">
              {{ getFileName(data) }}
            </span>
          </template>
        </Column>
      </DataTable>
    </template>
  </div>
</template>

<style scoped>
.feed-browser {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.feed-browser__form {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.feed-browser__editing {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 8px;
  background: rgba(0, 180, 216, 0.1);
  border: 1px solid rgba(0, 180, 216, 0.25);
  border-radius: 2px;
  font-size: 11px;
  color: #888;
}

.feed-browser__editing i {
  font-size: 10px;
  color: #00b4d8;
}

.feed-browser__editing strong {
  color: #00b4d8;
  font-weight: 500;
}

.feed-browser__field {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.feed-browser__label {
  font-size: 10px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: #666;
  cursor: help;
  transition: color 0.12s ease;
}

.feed-browser__field:hover .feed-browser__label {
  color: #888;
}

.feed-browser__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  padding-top: 8px;
  border-top: 1px solid #2a2a2a;
}

.feed-browser__empty {
  color: #555;
  font-size: 12px;
  padding: 12px 0;
  text-align: center;
  font-style: italic;
}

.feed-browser__message {
  margin: 0;
}

.feed-browser__table {
  margin-top: 6px;
}

.feed-browser__name {
  display: flex;
  align-items: center;
  gap: 6px;
}

.feed-browser__name--editing {
  color: #00b4d8;
}

.feed-browser__edit-icon {
  font-size: 10px;
  color: #00b4d8;
}

.feed-browser__source {
  color: #888;
  font-size: 11px;
  max-width: 120px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  display: block;
}

.feed-browser__success {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 8px;
  background: rgba(52, 199, 89, 0.1);
  border: 1px solid rgba(52, 199, 89, 0.25);
  border-radius: 2px;
  font-size: 11px;
  color: #34c759;
}

.feed-browser__success i {
  font-size: 12px;
}

.feed-browser__success-enter-active,
.feed-browser__success-leave-active {
  transition: opacity 0.2s ease;
}

.feed-browser__success-enter-from,
.feed-browser__success-leave-to {
  opacity: 0;
}
</style>
