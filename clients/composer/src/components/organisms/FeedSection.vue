<script setup lang="ts">
import { computed, ref } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import InputText from "primevue/inputtext";
import InputNumber from "primevue/inputnumber";
import Message from "primevue/message";
import Slider from "primevue/slider";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useFeedStore } from "../../stores/feedStore";
import { useAssetStore } from "../../stores/assetStore";
import { createId } from "../../composables/useIds";
import type { Feed, PanDirection } from "../../types/feed";
import { defaultFeedSettings, panDirectionLabels } from "../../types/feed";

const projectStore = useProjectStore();
const feedStore = useFeedStore();
const assetStore = useAssetStore();
const { feeds, activeFeed, isLoading, error } = storeToRefs(feedStore);
const { activeAsset } = storeToRefs(assetStore);

const name = ref("");
const panDirection = ref<PanDirection>(defaultFeedSettings.panDirection);
const panDurationSeconds = ref(defaultFeedSettings.panDurationSeconds);
const visiblePortion = ref(defaultFeedSettings.visiblePortion * 100); // Stored as percentage for slider

// Pan direction options
const panDirectionOptions = Object.entries(panDirectionLabels).map(([value, label]) => ({
  value: value as PanDirection,
  label,
}));

const selectedAssetPath = computed(() => activeAsset.value?.path ?? "");
const selectedAssetName = computed(() => activeAsset.value?.name ?? "");
const selectedAssetType = computed(() => activeAsset.value?.type ?? "VideoFile");
const assetById = computed(() => new Map(assetStore.assets.map((asset) => [asset.id, asset])));
const getAssetTypeForFeed = (feed: Feed) => assetById.value.get(feed.assetId)?.type ?? "VideoFile";

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

  const settings = {
    variantPath: "",
    monochrome: false,
    panDirection: panDirection.value,
    panDurationSeconds: panDurationSeconds.value,
    visiblePortion: visiblePortion.value / 100,
  };

  const payload: Feed = {
    projectId: projectStore.activeProject.id,
    id: createId("feed"),
    name: name.value.trim(),
    assetId: activeAsset.value?.id ?? "",
    settings,
  };

  await feedStore.createFeed(payload);
  name.value = "";
  // Reset pan settings to defaults
  panDirection.value = defaultFeedSettings.panDirection;
  panDurationSeconds.value = defaultFeedSettings.panDurationSeconds;
  visiblePortion.value = defaultFeedSettings.visiblePortion * 100;
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
        <div class="feed-section__asset-display">
          <template v-if="activeAsset">
            <i :class="selectedAssetType === 'ImageFile' ? 'pi pi-image' : 'pi pi-video'" class="feed-section__asset-icon" />
            <span class="feed-section__asset-name">{{ selectedAssetName }}</span>
            <span :class="['feed-section__asset-type', `feed-section__asset-type--${selectedAssetType}`]">
              {{ selectedAssetType }}
            </span>
          </template>
          <span v-else class="feed-section__asset-placeholder">Select an asset from the browser</span>
        </div>
      </div>
      <div class="feed-section__type-row">
        <span class="feed-section__type-label">Asset Type:</span>
        <span :class="['feed-section__type-badge', `feed-section__type-badge--${selectedAssetType}`]">
          <i :class="selectedAssetType === 'ImageFile' ? 'pi pi-image' : 'pi pi-video'" />
          {{ selectedAssetType === 'ImageFile' ? 'Image' : 'Video' }}
        </span>
        <Button label="Add Feed" icon="pi pi-plus" :disabled="!canCreate" @click="createFeed" />
      </div>

      <!-- Pan settings for Image feeds -->
      <div v-if="selectedAssetType === 'ImageFile'" class="feed-section__pan-settings">
        <div class="feed-section__setting">
          <label class="feed-section__label">Pan Direction</label>
          <Select
            v-model="panDirection"
            :options="panDirectionOptions"
            optionLabel="label"
            optionValue="value"
            placeholder="Direction"
            class="feed-section__select"
          />
        </div>
        <div class="feed-section__setting">
          <label class="feed-section__label">Pan Duration</label>
          <InputNumber
            v-model="panDurationSeconds"
            :min="10"
            :max="600"
            suffix=" sec"
            class="feed-section__number"
          />
        </div>
        <div class="feed-section__setting feed-section__setting--wide">
          <label class="feed-section__label">Visible Portion: {{ visiblePortion }}%</label>
          <Slider v-model="visiblePortion" :min="30" :max="100" class="feed-section__slider" />
        </div>
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
        <Column field="type" header="Type">
          <template #body="{ data }">
            <span :class="['feed-section__feed-type', `feed-section__feed-type--${getAssetTypeForFeed(data)}`]">
              <i :class="getAssetTypeForFeed(data) === 'ImageFile' ? 'pi pi-image' : 'pi pi-video'" />
              {{ getAssetTypeForFeed(data) === 'ImageFile' ? 'Image' : 'Video' }}
            </span>
          </template>
        </Column>
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
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 8px;
  align-items: center;
}

.feed-section__asset-display {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  background: #1a1a1a;
  border: 1px solid #2a2a2a;
  border-radius: 4px;
  min-height: 38px;
}

.feed-section__asset-icon {
  font-size: 1rem;
  opacity: 0.8;
}

.feed-section__asset-name {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.feed-section__asset-type {
  font-size: 0.75em;
  padding: 2px 6px;
  border-radius: 3px;
  text-transform: uppercase;
  font-weight: 500;
}

.feed-section__asset-type--image {
  background: #2d4a2d;
  color: #8fbc8f;
}

.feed-section__asset-type--video {
  background: #4a2d4a;
  color: #bc8fbc;
}

.feed-section__asset-type--unknown {
  background: #3a3a3a;
  color: #888;
}

.feed-section__asset-placeholder {
  color: #666;
  font-style: italic;
}

.feed-section__type-row {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
  padding: 8px 0;
}

.feed-section__type-label {
  font-size: 12px;
  color: #888;
}

.feed-section__type-select {
  min-width: 160px;
}

.feed-section__type-badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.85em;
  padding: 4px 10px;
  border-radius: 4px;
  text-transform: uppercase;
  font-weight: 500;
}

.feed-section__type-badge--VideoFile {
  background: #4a2d4a;
  color: #bc8fbc;
}

.feed-section__type-badge--ImageFile {
  background: #2d4a2d;
  color: #8fbc8f;
}

.feed-section__pan-settings {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  padding: 8px 12px;
  background: #1a1a1a;
  border-radius: 4px;
  border: 1px solid #2a2a2a;
}

.feed-section__setting {
  display: flex;
  align-items: center;
  gap: 8px;
}

.feed-section__setting--wide {
  flex-basis: 100%;
}

.feed-section__label {
  font-size: 12px;
  color: #888;
  min-width: 90px;
}

.feed-section__select {
  min-width: 160px;
}

.feed-section__number {
  width: 100px;
}

.feed-section__slider {
  flex: 1;
  min-width: 150px;
}

.feed-section__empty {
  color: #4a4640;
}

.feed-section__message {
  margin: 0;
}

.feed-section__feed-type {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.85em;
  padding: 2px 8px;
  border-radius: 4px;
  text-transform: uppercase;
  font-weight: 500;
}

.feed-section__feed-type--VideoFile {
  background: #4a2d4a;
  color: #bc8fbc;
}

.feed-section__feed-type--ImageFile {
  background: #2d4a2d;
  color: #8fbc8f;
}
</style>
