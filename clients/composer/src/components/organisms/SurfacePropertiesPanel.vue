<script setup lang="ts">
import { computed, nextTick, reactive, ref, watch } from "vue";
import { storeToRefs } from "pinia";
import Dropdown from "primevue/dropdown";
import InputNumber from "primevue/inputnumber";
import InputText from "primevue/inputtext";
import Message from "primevue/message";
import Button from "primevue/button";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";
import { useRendererStore } from "../../stores/rendererStore";
import type { Surface } from "../../types/surface";

const sceneStore = useSceneStore();
const feedStore = useFeedStore();
const rendererStore = useRendererStore();
const { activeScene, activeSurfaceId, error } = storeToRefs(sceneStore);

const isSyncing = ref(false);
const saveTimer = ref<ReturnType<typeof setTimeout> | null>(null);

const form = reactive({
  name: "",
  feedId: "",
  opacity: 1,
  brightness: 1,
  blendMode: "Normal" as Surface["blendMode"],
  zOrder: 0,
});

const feedOptions = computed(() =>
  feedStore.feeds.map((feed) => ({ label: feed.name, value: feed.id })),
);

const blendOptions: Surface["blendMode"][] = ["Normal", "Additive", "Multiply"];

const activeSurface = computed(() => {
  if (!activeScene.value || !activeSurfaceId.value) {
    return null;
  }
  return activeScene.value.surfaces.find((surface) => surface.id === activeSurfaceId.value) ?? null;
});

const canPreview = computed(
  () =>
    Boolean(activeScene.value && activeSurface.value?.feedId) &&
    !rendererStore.isLoading,
);

const syncForm = (surface: Surface | null) => {
  isSyncing.value = true;
  if (!surface) {
    form.name = "";
    form.feedId = "";
    form.opacity = 1;
    form.brightness = 1;
    form.blendMode = "Normal";
    form.zOrder = 0;
  } else {
    form.name = surface.name;
    form.feedId = surface.feedId;
    form.opacity = surface.opacity;
    form.brightness = surface.brightness;
    form.blendMode = surface.blendMode;
    form.zOrder = surface.zOrder;
  }
  void nextTick(() => {
    isSyncing.value = false;
  });
};

const scheduleSave = () => {
  if (isSyncing.value || !activeScene.value || !activeSurface.value) {
    return;
  }

  if (saveTimer.value) {
    clearTimeout(saveTimer.value);
  }

  saveTimer.value = setTimeout(async () => {
    if (!activeScene.value || !activeSurface.value) {
      return;
    }

    const nextSurfaces = activeScene.value.surfaces.map((surface) =>
      surface.id === activeSurface.value?.id
        ? {
            ...surface,
            name: form.name.trim(),
            feedId: form.feedId,
            opacity: form.opacity,
            brightness: form.brightness,
            blendMode: form.blendMode,
            zOrder: Math.round(form.zOrder),
          }
        : surface,
    );

    const nextScene = {
      ...activeScene.value,
      surfaces: nextSurfaces,
    };

    await sceneStore.updateScene(nextScene);
  }, 350);
};

watch(activeSurface, (surface) => syncForm(surface), { immediate: true });
watch(form, () => scheduleSave(), { deep: true });

const previewFeed = async () => {
  if (!activeScene.value) {
    return;
  }
  await rendererStore.loadScene(activeScene.value.projectId, activeScene.value.id);
};
</script>

<template>
  <div class="surface-properties">
    <div v-if="!activeSurface" class="surface-properties__empty">
      Select a surface to edit its properties.
    </div>

    <template v-else>
      <div class="surface-properties__group">
        <label class="surface-properties__label">Name</label>
        <InputText v-model="form.name" />
      </div>

      <div class="surface-properties__group">
        <label class="surface-properties__label">Feed</label>
        <Dropdown
          v-model="form.feedId"
          :options="feedOptions"
          optionLabel="label"
          optionValue="value"
          placeholder="Select feed"
          appendTo="self"
        />
      </div>

      <div class="surface-properties__row">
        <div class="surface-properties__group">
          <label class="surface-properties__label">Opacity</label>
          <InputNumber v-model="form.opacity" :min="0" :max="1" :step="0.05" />
        </div>
        <div class="surface-properties__group">
          <label class="surface-properties__label">Brightness</label>
          <InputNumber v-model="form.brightness" :min="0" :max="1" :step="0.05" />
        </div>
      </div>

      <div class="surface-properties__row">
        <div class="surface-properties__group">
          <label class="surface-properties__label">Blend</label>
          <Dropdown v-model="form.blendMode" :options="blendOptions" appendTo="self" />
        </div>
        <div class="surface-properties__group">
          <label class="surface-properties__label">Z Order</label>
          <InputNumber v-model="form.zOrder" :min="0" :max="999" :step="1" />
        </div>
      </div>

      <div class="surface-properties__actions">
        <Button
          label="Preview Feed"
          icon="pi pi-play"
          :disabled="!canPreview"
          :loading="rendererStore.isLoading"
          @click="previewFeed"
        />
      </div>
    </template>

    <Message v-if="error" severity="error" class="surface-properties__message">
      {{ error }}
    </Message>
  </div>
</template>

<style scoped>
.surface-properties {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.surface-properties__empty {
    color: #cbbfad;
}

.surface-properties__group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.surface-properties__label {
  font-size: 0.8rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #5b564f;
}

.surface-properties__row {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 12px;
}

.surface-properties__message {
  margin: 0;
}

.surface-properties__actions {
  display: flex;
  justify-content: flex-start;
}
</style>
