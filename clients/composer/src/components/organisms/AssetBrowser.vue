<script setup lang="ts">
import { computed, onMounted, ref } from "vue";
import { storeToRefs } from "pinia";
import DataTable from "primevue/datatable";
import Column from "primevue/column";
import InputText from "primevue/inputtext";
import FileUpload, { type FileUploadUploaderEvent } from "primevue/fileupload";
import Button from "primevue/button";
import Message from "primevue/message";
import { useAssetStore } from "../../stores/assetStore";

const assetStore = useAssetStore();
const { assets, activeAsset, error } = storeToRefs(assetStore);
const search = ref("");
const maxFileSize = 2 * 1024 * 1024 * 1024;

const filteredAssets = computed(() => {
  const term = search.value.trim().toLowerCase();
  if (!term) {
    return assets.value;
  }
  return assets.value.filter((asset) =>
    `${asset.name} ${asset.path}`.toLowerCase().includes(term),
  );
});

onMounted(() => {
  if (!assets.value.length) {
    assetStore.fetchAssets();
  }
});

const handleUpload = async (event: FileUploadUploaderEvent) => {
  if (!event.files || !event.files.length) {
    return;
  }

  for (const file of event.files) {
    await assetStore.uploadAsset(file);
  }
};

const deleteSelected = async () => {
  if (!activeAsset.value) {
    return;
  }
  const confirmed = window.confirm(`Delete asset ${activeAsset.value.name}?`);
  if (!confirmed) {
    return;
  }
  await assetStore.deleteAsset(activeAsset.value.id);
};
</script>

<template>
  <div class="asset-browser">
    <div class="asset-browser__toolbar">
      <InputText v-model="search" placeholder="Search assets" />
      <FileUpload
        mode="basic"
        name="file"
        accept="video/*,image/*"
        :maxFileSize="maxFileSize"
        :customUpload="true"
        :auto="true"
        chooseLabel="Upload"
        data-testid="asset-upload"
        @uploader="handleUpload"
      />
      <Button
        label="Delete"
        icon="pi pi-trash"
        text
        :disabled="!activeAsset"
        @click="deleteSelected"
      />
      <Button label="Refresh" icon="pi pi-refresh" text @click="assetStore.fetchAssets" />
    </div>

    <Message
      v-if="error"
      severity="error"
      class="asset-browser__message"
    >
      {{ error }}
    </Message>

    <DataTable
      :value="filteredAssets"
      selectionMode="single"
      dataKey="id"
      v-model:selection="activeAsset"
      responsiveLayout="scroll"
      class="asset-browser__table"
      size="small"
    >
      <Column field="name" header="Asset">
        <template #body="{ data }">
          <span class="asset-browser__name">
            <i
              :class="data.type === 'image' ? 'pi pi-image' : 'pi pi-video'"
              class="asset-browser__icon"
            />
            {{ data.name }}
          </span>
        </template>
      </Column>
      <Column field="type" header="Type">
        <template #body="{ data }">
          <span :class="['asset-browser__type', `asset-browser__type--${data.type}`]">
            {{ data.type === 'image' ? 'Image' : data.type === 'video' ? 'Video' : 'Unknown' }}
          </span>
        </template>
      </Column>
      <Column field="path" header="Path" />
    </DataTable>
  </div>
</template>

<style scoped>
.asset-browser {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.asset-browser__toolbar {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  align-items: center;
  padding-bottom: 8px;
  border-bottom: 1px solid #2a2a2a;
}

.asset-browser__table :deep(.p-datatable-tbody > tr) {
  cursor: pointer;
}

.asset-browser__message {
  margin: 0;
}

.asset-browser__name {
  display: flex;
  align-items: center;
  gap: 8px;
}

.asset-browser__icon {
  font-size: 1rem;
  opacity: 0.8;
}

.asset-browser__type {
  font-size: 0.85em;
  padding: 2px 8px;
  border-radius: 4px;
  text-transform: uppercase;
  font-weight: 500;
}

.asset-browser__type--image {
  background: #2d4a2d;
  color: #8fbc8f;
}

.asset-browser__type--video {
  background: #4a2d4a;
  color: #bc8fbc;
}

.asset-browser__type--unknown {
  background: #3a3a3a;
  color: #888;
}
</style>
