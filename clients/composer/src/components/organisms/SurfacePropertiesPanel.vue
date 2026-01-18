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
  rotation: 0,
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
    form.rotation = 0;
  } else {
    form.name = surface.name;
    form.feedId = surface.feedId;
    form.opacity = surface.opacity;
    form.brightness = surface.brightness;
    form.blendMode = surface.blendMode;
    form.zOrder = surface.zOrder;
    form.rotation = surface.rotation ?? 0;
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
            rotation: form.rotation,
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
      <i class="pi pi-sliders-h surface-properties__empty-icon"></i>
      <p>No surface selected</p>
      <span class="surface-properties__empty-hint">Click on a surface in the Output Preview to edit its properties.</span>
    </div>

    <template v-else>
      <div class="surface-properties__section">
        <div class="surface-properties__section-title">Basic</div>

        <div class="surface-properties__group">
          <label class="surface-properties__label" for="surface-name" title="Display name for this surface">
            Name
          </label>
          <InputText
            id="surface-name"
            v-model="form.name"
            placeholder="Enter surface name"
            title="Give this surface a descriptive name"
          />
        </div>

        <div class="surface-properties__group">
          <label class="surface-properties__label" for="surface-feed" title="Video feed to display on this surface">
            Video Feed
          </label>
          <Dropdown
            id="surface-feed"
            v-model="form.feedId"
            :options="feedOptions"
            optionLabel="label"
            optionValue="value"
            placeholder="Select a feed"
            appendTo="self"
            title="Choose which video feed to map onto this surface"
          />
        </div>
      </div>

      <div class="surface-properties__section">
        <div class="surface-properties__section-title">Appearance</div>

        <div class="surface-properties__row">
          <div class="surface-properties__group">
            <label class="surface-properties__label" for="surface-opacity" title="Transparency level (0 = invisible, 1 = fully opaque)">
              Opacity
            </label>
            <InputNumber
              id="surface-opacity"
              v-model="form.opacity"
              :min="0"
              :max="1"
              :step="0.05"
              title="0 = fully transparent, 1 = fully opaque"
            />
          </div>
          <div class="surface-properties__group">
            <label class="surface-properties__label" for="surface-brightness" title="Brightness multiplier (0 = black, 1 = full brightness)">
              Brightness
            </label>
            <InputNumber
              id="surface-brightness"
              v-model="form.brightness"
              :min="0"
              :max="1"
              :step="0.05"
              title="0 = completely dark, 1 = full brightness"
            />
          </div>
        </div>

        <div class="surface-properties__row">
          <div class="surface-properties__group">
            <label class="surface-properties__label" for="surface-blend" title="How this surface blends with layers below">
              Blend Mode
            </label>
            <Dropdown
              id="surface-blend"
              v-model="form.blendMode"
              :options="blendOptions"
              appendTo="self"
              title="Normal: standard overlay, Additive: brightens, Multiply: darkens"
            />
          </div>
          <div class="surface-properties__group">
            <label class="surface-properties__label" for="surface-zorder" title="Layer order (higher values render on top)">
              Z-Order
            </label>
            <InputNumber
              id="surface-zorder"
              v-model="form.zOrder"
              :min="0"
              :max="999"
              :step="1"
              title="Higher values render on top of lower values"
            />
          </div>
        </div>

        <div class="surface-properties__group">
          <label class="surface-properties__label" for="surface-rotation" title="Rotate the video content within the surface (in degrees)">
            Video Rotation
          </label>
          <div class="surface-properties__rotation-row">
            <InputNumber
              id="surface-rotation"
              v-model="form.rotation"
              :min="-360"
              :max="360"
              :step="15"
              suffix="°"
              title="Rotate video content: -360° to 360°, use 90° increments for cardinal directions"
            />
            <div class="surface-properties__rotation-presets">
              <Button icon="pi pi-replay" text size="small" title="Reset to 0°" @click="form.rotation = 0" />
              <Button label="90°" text size="small" title="Rotate 90° clockwise" @click="form.rotation = 90" />
              <Button label="180°" text size="small" title="Rotate 180°" @click="form.rotation = 180" />
              <Button label="-90°" text size="small" title="Rotate 90° counter-clockwise" @click="form.rotation = -90" />
            </div>
          </div>
        </div>
      </div>

      <div class="surface-properties__section">
        <div class="surface-properties__section-title">Actions</div>
        <div class="surface-properties__actions">
          <Button
            label="Send to Renderer"
            icon="pi pi-play"
            :disabled="!canPreview"
            :loading="rendererStore.isLoading"
            title="Send the current scene to the connected renderer for preview"
            @click="previewFeed"
          />
        </div>
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
  gap: 12px;
  padding: 10px;
}

.surface-properties__empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 32px 16px;
  color: #555;
}

.surface-properties__empty-icon {
  font-size: 1.8rem;
  color: rgba(0, 180, 216, 0.2);
  margin-bottom: 10px;
}

.surface-properties__empty p {
  margin: 0 0 6px;
  font-size: 12px;
  font-weight: 500;
  color: #777;
}

.surface-properties__empty-hint {
  font-size: 11px;
  color: #555;
  max-width: 200px;
  line-height: 1.4;
}

.surface-properties__section {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding-bottom: 10px;
  border-bottom: 1px solid #2a2a2a;
}

.surface-properties__section:last-of-type {
  border-bottom: none;
  padding-bottom: 0;
}

.surface-properties__section-title {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #00b4d8;
  font-weight: 500;
  margin-bottom: 0;
}

.surface-properties__group {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.surface-properties__label {
  font-size: 10px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: #666;
  cursor: help;
  transition: color 0.12s ease;
}

.surface-properties__group:hover .surface-properties__label {
  color: #888;
}

.surface-properties__row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
}

.surface-properties__message {
  margin: 0;
}

.surface-properties__actions {
  display: flex;
  justify-content: flex-start;
  padding-top: 2px;
}

.surface-properties__rotation-row {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.surface-properties__rotation-presets {
  display: flex;
  gap: 4px;
  flex-wrap: wrap;
}

.surface-properties__rotation-presets :deep(.p-button) {
  padding: 4px 8px;
  font-size: 11px;
  font-weight: 500;
}

/* Input styling overrides */
:deep(.p-inputtext),
:deep(.p-dropdown),
:deep(.p-inputnumber-input) {
  width: 100%;
}

:deep(.p-inputnumber) {
  width: 100%;
}
</style>
