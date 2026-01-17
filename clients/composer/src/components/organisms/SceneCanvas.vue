<script setup lang="ts">
import { computed, reactive, ref } from "vue";
import { storeToRefs } from "pinia";
import SpeedDial from "primevue/speeddial";
import Message from "primevue/message";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";
import { createSurface, type SurfaceShape } from "../../composables/useSurfaceFactory";
import type { Surface, Vec2 } from "../../types/surface";

const sceneStore = useSceneStore();
const feedStore = useFeedStore();
const { activeScene, activeSurfaceId, error } = storeToRefs(sceneStore);

const viewportRef = ref<HTMLDivElement | null>(null);
const zoom = ref(0.25);
const panX = ref(0);
const panY = ref(0);
const isPanning = ref(false);
const panStartX = ref(0);
const panStartY = ref(0);
const panOriginX = ref(0);
const panOriginY = ref(0);
const panMoved = ref(false);
const suppressClear = ref(false);
const dragState = reactive<{
  mode: "shape" | "vertex" | null;
  surfaceId: string | null;
  vertexIndex: number | null;
  startPointer: Vec2 | null;
  startVertices: Vec2[];
}>({
  mode: null,
  surfaceId: null,
  vertexIndex: null,
  startPointer: null,
  startVertices: [],
});

const stageWidth = 1920;
const stageHeight = 1080;
const zoomLabel = computed(() => `${Math.round(zoom.value * 100)}%`);
const hasActiveScene = computed(() => Boolean(activeScene.value));
const hasFeeds = computed(() => feedStore.feeds.length > 0);
const canAddShape = computed(() => hasActiveScene.value && hasFeeds.value && !sceneStore.isLoading);
const activeSurface = computed(
  () => activeScene.value?.surfaces.find((surface) => surface.id === activeSurfaceId.value) ?? null,
);

const resolveFeedId = () =>
  feedStore.activeFeed?.id ?? feedStore.feeds[0]?.id ?? "";

const addSurface = async (shape: SurfaceShape) => {
  if (!activeScene.value || !canAddShape.value) {
    return;
  }

  const feedId = resolveFeedId();
  if (!feedId) {
    return;
  }

  const nextIndex = activeScene.value.surfaces.length + 1;
  const newSurface = createSurface(shape, {
    feedId,
    zOrder: activeScene.value.surfaces.length,
    index: nextIndex,
  });

  const nextScene = {
    ...activeScene.value,
    surfaces: [...activeScene.value.surfaces, newSurface],
  };

  await sceneStore.updateScene(nextScene);
  sceneStore.setActiveSurfaceId(newSurface.id);
};

const shapeActions = computed(() => [
  {
    label: "Add Rectangle",
    icon: "pi pi-stop",
    command: () => addSurface("rectangle"),
  },
  {
    label: "Add Quad",
    icon: "pi pi-clone",
    command: () => addSurface("quad"),
  },
]);

const clamp = (value: number, min: number, max: number) =>
  Math.min(max, Math.max(min, value));

const clientToStage = (event: PointerEvent | WheelEvent) => {
  if (!viewportRef.value) {
    return { x: 0, y: 0 };
  }
  const rect = viewportRef.value.getBoundingClientRect();
  const x = (event.clientX - rect.left - panX.value) / zoom.value;
  const y = (event.clientY - rect.top - panY.value) / zoom.value;
  return { x, y };
};

const stageToNormalized = (stage: Vec2): Vec2 => ({
  x: clamp((stage.x / stageWidth - 0.5) * 2, -1, 1),
  y: clamp((stage.y / stageHeight - 0.5) * 2, -1, 1),
});

const clientToNormalized = (event: PointerEvent | WheelEvent): Vec2 =>
  stageToNormalized(clientToStage(event));

const onWheel = (event: WheelEvent) => {
  if (!viewportRef.value) {
    return;
  }

  const direction = event.deltaY > 0 ? -1 : 1;
  const nextZoom = clamp(zoom.value + direction * 0.05, 0.1, 2);
  if (nextZoom === zoom.value) {
    return;
  }

  const rect = viewportRef.value.getBoundingClientRect();
  const cursorX = event.clientX - rect.left;
  const cursorY = event.clientY - rect.top;
  const scale = nextZoom / zoom.value;

  panX.value = cursorX - scale * (cursorX - panX.value);
  panY.value = cursorY - scale * (cursorY - panY.value);
  zoom.value = nextZoom;
};

const startPan = (event: PointerEvent) => {
  if (dragState.mode) {
    return;
  }
  if (event.button !== 0) {
    return;
  }
  isPanning.value = true;
  panMoved.value = false;
  panStartX.value = event.clientX;
  panStartY.value = event.clientY;
  panOriginX.value = panX.value;
  panOriginY.value = panY.value;
  (event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
};

const movePan = (event: PointerEvent) => {
  if (dragState.mode) {
    return;
  }
  if (!isPanning.value) {
    return;
  }
  const dx = event.clientX - panStartX.value;
  const dy = event.clientY - panStartY.value;
  if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
    panMoved.value = true;
  }
  panX.value = panOriginX.value + dx;
  panY.value = panOriginY.value + dy;
};

const endPan = () => {
  isPanning.value = false;
};

const findSurface = (surfaceId: string) =>
  activeScene.value?.surfaces.find((surface) => surface.id === surfaceId) ?? null;

const updateSurfaceVertices = (surfaceId: string, vertices: Vec2[]) => {
  const surface = findSurface(surfaceId);
  if (!surface) {
    return;
  }
  surface.vertices = vertices;
};

const startShapeDrag = (surface: Surface, event: PointerEvent) => {
  if (event.button !== 0) {
    return;
  }
  event.preventDefault();
  suppressClear.value = true;
  dragState.mode = "shape";
  dragState.surfaceId = surface.id;
  dragState.vertexIndex = null;
  dragState.startPointer = clientToNormalized(event);
  dragState.startVertices = surface.vertices.map((vertex) => ({ ...vertex }));
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

const startVertexDrag = (surface: Surface, index: number, event: PointerEvent) => {
  if (event.button !== 0) {
    return;
  }
  event.preventDefault();
  suppressClear.value = true;
  dragState.mode = "vertex";
  dragState.surfaceId = surface.id;
  dragState.vertexIndex = index;
  dragState.startPointer = clientToNormalized(event);
  dragState.startVertices = surface.vertices.map((vertex) => ({ ...vertex }));
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

const moveDrag = (event: PointerEvent) => {
  if (!dragState.mode || !dragState.surfaceId || !dragState.startPointer) {
    return;
  }
  const current = clientToNormalized(event);
  const delta = {
    x: current.x - dragState.startPointer.x,
    y: current.y - dragState.startPointer.y,
  };

  const nextVertices = dragState.startVertices.map((vertex, index) => {
    if (dragState.mode === "vertex" && dragState.vertexIndex !== index) {
      return vertex;
    }
    return {
      x: clamp(vertex.x + delta.x, -1, 1),
      y: clamp(vertex.y + delta.y, -1, 1),
    };
  });

  updateSurfaceVertices(dragState.surfaceId, nextVertices);
};

const endDrag = async () => {
  if (dragState.mode && activeScene.value) {
    await sceneStore.updateScene({ ...activeScene.value });
  }
  dragState.mode = null;
  dragState.surfaceId = null;
  dragState.vertexIndex = null;
  dragState.startPointer = null;
  dragState.startVertices = [];
};

const clearSelection = () => {
  if (suppressClear.value) {
    suppressClear.value = false;
    return;
  }
  if (panMoved.value) {
    panMoved.value = false;
    return;
  }
  sceneStore.setActiveSurfaceId(null);
};

const selectSurface = (surfaceId: string) => {
  sceneStore.setActiveSurfaceId(surfaceId);
};

const toStage = (x: number, y: number) => ({
  x: (x * 0.5 + 0.5) * stageWidth,
  y: (y * 0.5 + 0.5) * stageHeight,
});

const toPoint = (x: number, y: number) => {
  const point = toStage(x, y);
  return `${point.x},${point.y}`;
};

const surfacePoints = (shape: { vertices: { x: number; y: number }[] }) =>
  shape.vertices.map((vertex) => toPoint(vertex.x, vertex.y)).join(" ");
</script>

<template>
  <div class="scene-canvas">
    <div class="scene-canvas__header">
      <div class="scene-canvas__title">Scene Canvas</div>
      <div class="scene-canvas__meta" data-testid="zoom-label">Zoom: {{ zoomLabel }}</div>
    </div>

    <div v-if="!hasActiveScene" class="scene-canvas__empty">
      Select or create a scene to start mapping surfaces.
    </div>

    <div v-else class="scene-canvas__workspace">
      <div
        ref="viewportRef"
        class="scene-canvas__viewport"
        data-testid="scene-viewport"
        @wheel.prevent="onWheel"
        @pointerdown="startPan"
        @pointermove="
          (event) => {
            if (dragState.mode) {
              moveDrag(event);
              return;
            }
            movePan(event);
          }
        "
        @pointerup="
          async () => {
            endPan();
            await endDrag();
          }
        "
        @pointercancel="
          async () => {
            endPan();
            await endDrag();
          }
        "
        @click="clearSelection"
      >
        <div
          class="scene-canvas__stage"
          :style="{
            transform: `translate(${panX}px, ${panY}px) scale(${zoom})`,
          }"
        >
          <svg
            class="scene-canvas__surfaces"
            :width="stageWidth"
            :height="stageHeight"
            viewBox="0 0 1920 1080"
            role="presentation"
          >
            <polygon
              v-for="surface in activeScene?.surfaces ?? []"
              :key="surface.id"
              :points="surfacePoints(surface)"
              :data-surface-id="surface.id"
              class="scene-canvas__surface"
              :class="{ 'scene-canvas__surface--active': surface.id === activeSurfaceId }"
              @click.stop="selectSurface(surface.id)"
              @pointerdown.stop="startShapeDrag(surface, $event)"
            />
            <circle
              v-for="(vertex, index) in activeSurface?.vertices ?? []"
              :key="`handle-${index}`"
              class="scene-canvas__handle"
              :cx="toStage(vertex.x, vertex.y).x"
              :cy="toStage(vertex.x, vertex.y).y"
              r="10"
              @pointerdown.stop="startVertexDrag(activeSurface, index, $event)"
              @click.stop
            />
          </svg>
        </div>

        <div class="scene-canvas__toolbar" @pointerdown.stop @click.stop>
          <SpeedDial
            :model="shapeActions"
            :disabled="!canAddShape"
            direction="down"
            showIcon="pi pi-plus"
            hideIcon="pi pi-times"
            buttonClass="scene-canvas__speed-dial"
          />
        </div>
      </div>

      <div v-if="!hasFeeds" class="scene-canvas__hint">
        Add a feed to enable shape creation.
      </div>

      <Message v-if="error" severity="error" class="scene-canvas__message">
        {{ error }}
      </Message>
    </div>
  </div>
</template>

<style scoped>
.scene-canvas {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.scene-canvas__header {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
}

.scene-canvas__title {
  font-weight: 600;
  letter-spacing: 0.02em;
}

.scene-canvas__meta {
  font-size: 0.85rem;
  color: #5b564f;
}

.scene-canvas__empty {
  color: #cbbfad;
  padding: 16px 8px;
}

.scene-canvas__workspace {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.scene-canvas__viewport {
  position: relative;
  width: 480px;
  height: 270px;
  margin: 0 auto;
  border-radius: 14px;
  background: radial-gradient(circle at top left, #242424, #141414 60%);
  border: 1px solid #2f2f2f;
  overflow: hidden;
  touch-action: none;
}

.scene-canvas__stage {
  position: relative;
  width: 1920px;
  height: 1080px;
  transform-origin: 0 0;
  background: #080808;
  border: 1px solid #2f2f2f;
}

.scene-canvas__surfaces {
  position: absolute;
  inset: 0;
}

.scene-canvas__surface {
  fill: rgba(255, 255, 255, 0.06);
  stroke: rgba(255, 255, 255, 0.45);
  stroke-width: 2;
  cursor: pointer;
}

.scene-canvas__surface--active {
  stroke: #f4c469;
  fill: rgba(244, 196, 105, 0.18);
}

.scene-canvas__handle {
  fill: #1c1a17;
  stroke: #f4c469;
  stroke-width: 2;
  cursor: grab;
}

.scene-canvas__toolbar {
  position: absolute;
  top: 12px;
  right: 12px;
  z-index: 2;
}

.scene-canvas__speed-dial :deep(.p-speeddial-button) {
  background: #f5f2e9;
  border: 1px solid #cfccc4;
  color: #2f2d2a;
}

.scene-canvas__hint {
  font-size: 0.85rem;
  color: #6b665f;
}

.scene-canvas__message {
  margin: 0;
}
</style>
