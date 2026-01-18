<script setup lang="ts">
import { computed, reactive, ref, watch, onMounted, onUnmounted, nextTick } from "vue";
import { storeToRefs } from "pinia";
import Message from "primevue/message";
import Button from "primevue/button";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";
import { useRendererStore } from "../../stores/rendererStore";
import { createSurface, type SurfaceShape } from "../../composables/useSurfaceFactory";
import type { Surface, Vec2, EllipseSurface } from "../../types/surface";
import { isEllipseSurface, getSurfaceVertices } from "../../types/surface";

const sceneStore = useSceneStore();
const feedStore = useFeedStore();
const rendererStore = useRendererStore();
const { activeScene, activeSurfaceId, error } = storeToRefs(sceneStore);

const viewportRef = ref<HTMLDivElement | null>(null);
const zoom = ref(0.25);
const panX = ref(0);
const panY = ref(0);
const isPanning = ref(false);
const panPointerId = ref<number | null>(null);
const panStartX = ref(0);
const panStartY = ref(0);
const panOriginX = ref(0);
const panOriginY = ref(0);
const panMoved = ref(false);
const suppressClear = ref(false);
const dragState = reactive<{
  mode: "shape" | "vertex" | "rotation" | "ellipse-center" | "ellipse-radius-x" | "ellipse-radius-y" | null;
  surfaceId: string | null;
  vertexIndex: number | null;
  startPointer: Vec2 | null;
  startVertices: Vec2[];
  startRotation: number;
  startAngle: number;
  // Ellipse-specific state
  startCenter: Vec2 | null;
  startRadiusX: number;
  startRadiusY: number;
  // Track pointer for capture release
  pointerId: number | null;
}>({
  mode: null,
  surfaceId: null,
  vertexIndex: null,
  startPointer: null,
  startVertices: [],
  startRotation: 0,
  startAngle: 0,
  startCenter: null,
  startRadiusX: 0,
  startRadiusY: 0,
  pointerId: null,
});

// Edge snapping configuration
const snapEnabled = ref(true);
const SNAP_THRESHOLD = 0.02; // Normalized distance threshold for snapping

// Projector calibration grid overlay
const showProjectorGrid = ref(false);

// Spacebar modifier for panning (hold spacebar + drag to pan)
const spacebarHeld = ref(false);

const stageWidth = 1920;
const stageHeight = 1080;
const zoomLabel = computed(() => `${Math.round(zoom.value * 100)}%`);
const hasActiveScene = computed(() => Boolean(activeScene.value));
const hasFeeds = computed(() => feedStore.feeds.length > 0);
const canAddShape = computed(() => hasActiveScene.value && hasFeeds.value && !sceneStore.isLoading);
const activeSurface = computed(
  () => activeScene.value?.surfaces.find((surface) => surface.id === activeSurfaceId.value) ?? null,
);

// Get feed name for a surface
const getFeedName = (feedId: string): string => {
  const feed = feedStore.feeds.find((f) => f.id === feedId);
  return feed?.name ?? "Unknown";
};

// Compute surface label info (center position and text)
const getSurfaceLabelInfo = (surface: Surface) => {
  let center: Vec2;

  if (isEllipseSurface(surface)) {
    center = surface.center;
  } else {
    const vertices = "vertices" in surface ? surface.vertices : [];
    if (!vertices || vertices.length === 0) return null;

    // Compute centroid
    const sum = vertices.reduce(
      (acc, v) => ({ x: acc.x + v.x, y: acc.y + v.y }),
      { x: 0, y: 0 }
    );
    center = {
      x: sum.x / vertices.length,
      y: sum.y / vertices.length,
    };
  }

  // Convert to stage coordinates
  const stagePos = toStage(center.x, center.y);

  return {
    x: stagePos.x,
    y: stagePos.y,
    name: surface.name || getFeedName(surface.feedId),
  };
};

const canPreview = computed(
  () => Boolean(activeScene.value) && !rendererStore.isLoading,
);

const livePreview = ref(false);
let previewDebounceTimer: ReturnType<typeof setTimeout> | null = null;

const previewScene = async () => {
  if (!activeScene.value) {
    return;
  }
  await rendererStore.loadScene(activeScene.value.projectId, activeScene.value.id);
};

// Debounced preview for live mode - prevents flooding the renderer
const debouncedPreview = () => {
  if (previewDebounceTimer) {
    clearTimeout(previewDebounceTimer);
  }
  previewDebounceTimer = setTimeout(() => {
    if (livePreview.value && canPreview.value) {
      void previewScene();
    }
  }, 150);
};

// Watch for scene changes when live preview is enabled
// Skip during vertex drag to avoid video flashing - scene updates on drag end
watch(
  () => activeScene.value?.surfaces,
  () => {
    if (livePreview.value && dragState.mode !== "vertex") {
      debouncedPreview();
    }
  },
  { deep: true },
);

// When live preview is enabled, immediately send the current scene to renderer
// We check activeScene.value directly instead of canPreview to avoid blocking
// on rendererStore.isLoading from unrelated operations like ping
watch(
  livePreview,
  async (isLive) => {
    if (isLive && activeScene.value) {
      try {
        await previewScene();
      } catch {
        // Error is already handled by the store
      }
    }
  },
  { flush: "post" },
);

// Keyboard shortcut handler
const handleKeyDown = (event: KeyboardEvent) => {
  // Track spacebar for pan modifier (works even in input fields)
  if (event.code === "Space" && !event.repeat) {
    spacebarHeld.value = true;
  }

  const target = event.target as HTMLElement;
  const isInputField = target.tagName === "INPUT" || target.tagName === "TEXTAREA";

  // Ctrl/Cmd + P to toggle live preview
  if ((event.ctrlKey || event.metaKey) && event.key === "p") {
    event.preventDefault();
    if (canPreview.value) {
      livePreview.value = !livePreview.value;
    }
    return;
  }

  // Skip shortcuts when focused on input fields
  if (isInputField) return;

  // Prevent spacebar from scrolling the page when used as pan modifier
  if (event.code === "Space") {
    event.preventDefault();
  }

  // F to fit to view
  if (event.key === "f" && !event.ctrlKey && !event.metaKey && !event.altKey) {
    event.preventDefault();
    fitToView();
  }
  // 0 (zero) to reset zoom to 100%
  if (event.key === "0" && !event.ctrlKey && !event.metaKey && !event.altKey) {
    event.preventDefault();
    zoom.value = 1;
    centerViewport();
  }
  // Delete or Backspace to delete selected surface
  if ((event.key === "Delete" || event.key === "Backspace") && activeSurfaceId.value) {
    event.preventDefault();
    void deleteSurface();
  }
  // Ctrl/Cmd + D to duplicate selected surface
  if ((event.ctrlKey || event.metaKey) && event.key === "d" && activeSurface.value) {
    event.preventDefault();
    void duplicateSurface();
  }
};

// Track spacebar release
const handleKeyUp = (event: KeyboardEvent) => {
  if (event.code === "Space") {
    spacebarHeld.value = false;
  }
};

// Center the stage in the viewport
const centerViewport = () => {
  if (!viewportRef.value) return;
  const rect = viewportRef.value.getBoundingClientRect();
  const scaledWidth = stageWidth * zoom.value;
  const scaledHeight = stageHeight * zoom.value;
  panX.value = (rect.width - scaledWidth) / 2;
  panY.value = (rect.height - scaledHeight) / 2;
};

// Fit stage to viewport with padding
const fitToView = () => {
  if (!viewportRef.value) return;
  const rect = viewportRef.value.getBoundingClientRect();
  const padding = 40; // Padding around the stage
  const availableWidth = rect.width - padding * 2;
  const availableHeight = rect.height - padding * 2;
  const scaleX = availableWidth / stageWidth;
  const scaleY = availableHeight / stageHeight;
  zoom.value = Math.min(scaleX, scaleY, 1); // Cap at 100%
  centerViewport();
};

// Global pointer event handlers for reliable drag end (even if pointer leaves viewport)
const handleGlobalPointerUp = async () => {
  if (dragState.mode) {
    await endDrag();
  }
  endPan();
};

// Global move handler for when pointer leaves viewport during drag
const handleGlobalPointerMove = (event: PointerEvent) => {
  // Only handle if we have an active drag/pan with a tracked pointer
  if (dragState.mode && dragState.pointerId === event.pointerId) {
    moveDrag(event);
  } else if (isPanning.value && panPointerId.value === event.pointerId) {
    movePan(event);
  }
};

onMounted(() => {
  window.addEventListener("keydown", handleKeyDown);
  window.addEventListener("keyup", handleKeyUp);
  // Global listeners ensure drag/pan ends even if pointer leaves the viewport
  window.addEventListener("pointerup", handleGlobalPointerUp);
  window.addEventListener("pointercancel", handleGlobalPointerUp);
  window.addEventListener("pointermove", handleGlobalPointerMove);
  // Center viewport after DOM is fully ready - use multiple attempts for reliability
  const attemptCenter = (attempts = 0) => {
    if (!viewportRef.value) {
      if (attempts < 10) {
        requestAnimationFrame(() => attemptCenter(attempts + 1));
      }
      return;
    }
    const rect = viewportRef.value.getBoundingClientRect();
    // Wait until viewport has dimensions
    if (rect.width === 0 || rect.height === 0) {
      if (attempts < 10) {
        requestAnimationFrame(() => attemptCenter(attempts + 1));
      }
      return;
    }
    centerViewport();
  };
  requestAnimationFrame(() => attemptCenter());
});

onUnmounted(() => {
  window.removeEventListener("keydown", handleKeyDown);
  window.removeEventListener("keyup", handleKeyUp);
  window.removeEventListener("pointerup", handleGlobalPointerUp);
  window.removeEventListener("pointercancel", handleGlobalPointerUp);
  window.removeEventListener("pointermove", handleGlobalPointerMove);
  if (previewDebounceTimer) {
    clearTimeout(previewDebounceTimer);
  }
});

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

const showShapeMenu = ref(false);

const toggleAddMenu = () => {
  showShapeMenu.value = !showShapeMenu.value;
};

// Close shape menu when clicking outside
const closeShapeMenu = () => {
  showShapeMenu.value = false;
};

const clamp = (value: number, min: number, max: number) =>
  Math.min(max, Math.max(min, value));

// Get all vertices from all surfaces except the current one (for vertex snapping)
const getOtherSurfaceVertices = (excludeSurfaceId: string): Vec2[] => {
  if (!activeScene.value) return [];
  const vertices: Vec2[] = [];
  for (const surface of activeScene.value.surfaces) {
    if (surface.id === excludeSurfaceId) continue;
    // Get vertices based on surface type
    if (isEllipseSurface(surface)) {
      // For ellipses, use generated vertices for snapping
      const ellipseVerts = getSurfaceVertices(surface, 16);
      vertices.push(...ellipseVerts);
    } else if ("vertices" in surface && surface.vertices) {
      vertices.push(...surface.vertices);
    }
  }
  return vertices;
};

// Get all edges from all surfaces except the current one (for edge snapping)
const getOtherSurfaceEdges = (excludeSurfaceId: string) => {
  if (!activeScene.value) return [];
  const edges: { p1: Vec2; p2: Vec2 }[] = [];
  for (const surface of activeScene.value.surfaces) {
    if (surface.id === excludeSurfaceId) continue;
    // Get vertices based on surface type
    let verts: Vec2[];
    if (isEllipseSurface(surface)) {
      verts = getSurfaceVertices(surface, 16);
    } else if ("vertices" in surface && surface.vertices) {
      verts = surface.vertices;
    } else {
      continue;
    }
    for (let i = 0; i < verts.length; i++) {
      edges.push({
        p1: verts[i],
        p2: verts[(i + 1) % verts.length],
      });
    }
  }
  return edges;
};

// Find the closest point on an edge to a given point
const closestPointOnEdge = (p: Vec2, edge: { p1: Vec2; p2: Vec2 }): Vec2 => {
  const dx = edge.p2.x - edge.p1.x;
  const dy = edge.p2.y - edge.p1.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq === 0) return edge.p1;

  const t = clamp(((p.x - edge.p1.x) * dx + (p.y - edge.p1.y) * dy) / lenSq, 0, 1);
  return {
    x: edge.p1.x + t * dx,
    y: edge.p1.y + t * dy,
  };
};

// Viewport boundary corners (normalized coordinates -1 to 1)
const VIEWPORT_CORNERS: Vec2[] = [
  { x: -1, y: -1 }, // Top-left
  { x: 1, y: -1 },  // Top-right
  { x: 1, y: 1 },   // Bottom-right
  { x: -1, y: 1 },  // Bottom-left
];

// Viewport boundary edges
const VIEWPORT_EDGES: { p1: Vec2; p2: Vec2 }[] = [
  { p1: { x: -1, y: -1 }, p2: { x: 1, y: -1 } },  // Top edge
  { p1: { x: 1, y: -1 }, p2: { x: 1, y: 1 } },    // Right edge
  { p1: { x: 1, y: 1 }, p2: { x: -1, y: 1 } },    // Bottom edge
  { p1: { x: -1, y: 1 }, p2: { x: -1, y: -1 } },  // Left edge
];

// Apply snapping to a vertex position (vertices take priority over edges)
const snapVertex = (vertex: Vec2, surfaceId: string): Vec2 => {
  if (!snapEnabled.value) return vertex;

  let snappedVertex = vertex;
  let minDist = SNAP_THRESHOLD;

  // First, try to snap to other surface vertices (highest priority)
  const otherVertices = getOtherSurfaceVertices(surfaceId);
  for (const otherVertex of otherVertices) {
    const dist = Math.sqrt(
      (vertex.x - otherVertex.x) ** 2 + (vertex.y - otherVertex.y) ** 2
    );
    if (dist < minDist) {
      minDist = dist;
      snappedVertex = { x: otherVertex.x, y: otherVertex.y };
    }
  }

  // If we found a vertex snap, use it (don't check edges)
  if (snappedVertex !== vertex) {
    return snappedVertex;
  }

  // Try to snap to viewport corners (second priority)
  for (const corner of VIEWPORT_CORNERS) {
    const dist = Math.sqrt(
      (vertex.x - corner.x) ** 2 + (vertex.y - corner.y) ** 2
    );
    if (dist < minDist) {
      minDist = dist;
      snappedVertex = { x: corner.x, y: corner.y };
    }
  }

  // If we found a corner snap, use it
  if (snappedVertex !== vertex) {
    return snappedVertex;
  }

  // Try to snap to other surface edges
  const edges = getOtherSurfaceEdges(surfaceId);
  for (const edge of edges) {
    const closest = closestPointOnEdge(vertex, edge);
    const dist = Math.sqrt(
      (vertex.x - closest.x) ** 2 + (vertex.y - closest.y) ** 2
    );
    if (dist < minDist) {
      minDist = dist;
      snappedVertex = closest;
    }
  }

  // If we found a surface edge snap, use it
  if (snappedVertex !== vertex) {
    return snappedVertex;
  }

  // Finally, try to snap to viewport edges (lowest priority)
  for (const edge of VIEWPORT_EDGES) {
    const closest = closestPointOnEdge(vertex, edge);
    const dist = Math.sqrt(
      (vertex.x - closest.x) ** 2 + (vertex.y - closest.y) ** 2
    );
    if (dist < minDist) {
      minDist = dist;
      snappedVertex = closest;
    }
  }

  return snappedVertex;
};

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
  x: (stage.x / stageWidth - 0.5) * 2,
  y: (stage.y / stageHeight - 0.5) * 2,
});

const clientToNormalized = (event: PointerEvent | WheelEvent): Vec2 =>
  stageToNormalized(clientToStage(event));

const onWheel = (event: WheelEvent) => {
  if (!viewportRef.value) {
    return;
  }

  // Use multiplicative zoom for smoother feel (1.08 = 8% per step)
  const zoomFactor = 1.08;
  const direction = event.deltaY > 0 ? 1 / zoomFactor : zoomFactor;
  const nextZoom = clamp(zoom.value * direction, 0.1, 2);
  if (Math.abs(nextZoom - zoom.value) < 0.001) {
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
  // When spacebar is held, allow panning from anywhere (ignore interactive elements check)
  if (!spacebarHeld.value) {
    // Don't start panning if we clicked on an interactive element (handle, surface, etc.)
    const target = event.target as Element;
    if (target.closest('.scene-canvas__rotation-handle, .scene-canvas__rotation-icon, .scene-canvas__handle, .scene-canvas__ellipse-handle, .scene-canvas__surface, .scene-canvas__surface-label')) {
      return;
    }
  }
  isPanning.value = true;
  panPointerId.value = event.pointerId;
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
  panPointerId.value = null;
};

const findSurface = (surfaceId: string) =>
  activeScene.value?.surfaces.find((surface) => surface.id === surfaceId) ?? null;

const updateSurfaceVertices = (surfaceId: string, vertices: Vec2[]) => {
  const surface = findSurface(surfaceId);
  if (!surface || !("vertices" in surface)) {
    return;
  }
  surface.vertices = vertices;
};

const startShapeDrag = (surface: Surface, event: PointerEvent) => {
  if (event.button !== 0) {
    return;
  }
  // If spacebar is held, pan instead of dragging the surface
  if (spacebarHeld.value) {
    startPan(event);
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  suppressClear.value = true;

  if (isEllipseSurface(surface)) {
    // For ellipse surfaces, dragging the shape moves its center
    dragState.mode = "ellipse-center";
    dragState.surfaceId = surface.id;
    dragState.startPointer = clientToNormalized(event);
    dragState.startCenter = { ...surface.center };
    dragState.startRadiusX = surface.radiusX;
    dragState.startRadiusY = surface.radiusY;
  } else {
    // Polygon surface - get vertices safely
    const vertices = "vertices" in surface ? surface.vertices : [];
    dragState.mode = "shape";
    dragState.surfaceId = surface.id;
    dragState.vertexIndex = null;
    dragState.startPointer = clientToNormalized(event);
    dragState.startVertices = vertices.map((vertex) => ({ ...vertex }));
  }
  dragState.pointerId = event.pointerId;
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

const startVertexDrag = (surface: Surface, index: number, event: PointerEvent) => {
  if (event.button !== 0) {
    return;
  }
  // If spacebar is held, pan instead of dragging the vertex
  if (spacebarHeld.value) {
    startPan(event);
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  suppressClear.value = true;

  // Get vertices - surface must be a polygon type here
  const vertices = "vertices" in surface ? surface.vertices : [];
  if (vertices.length === 0) {
    return;
  }

  dragState.mode = "vertex";
  dragState.surfaceId = surface.id;
  dragState.vertexIndex = index;
  dragState.startPointer = clientToNormalized(event);
  dragState.startVertices = vertices.map((vertex) => ({ ...vertex }));
  dragState.pointerId = event.pointerId;
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

// Calculate normalized center of a surface
const getSurfaceCenterNorm = (surface: Surface): Vec2 => {
  if (isEllipseSurface(surface)) {
    return surface.center;
  }
  const vertices = "vertices" in surface ? surface.vertices : [];
  if (!vertices || vertices.length === 0) {
    return { x: 0, y: 0 };
  }
  const sumX = vertices.reduce((acc, v) => acc + v.x, 0);
  const sumY = vertices.reduce((acc, v) => acc + v.y, 0);
  return {
    x: sumX / vertices.length,
    y: sumY / vertices.length,
  };
};

// Start dragging ellipse radius-X handle
const startEllipseRadiusXDrag = (surface: EllipseSurface, event: PointerEvent) => {
  if (event.button !== 0) return;
  if (spacebarHeld.value) {
    startPan(event);
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  suppressClear.value = true;
  dragState.mode = "ellipse-radius-x";
  dragState.surfaceId = surface.id;
  dragState.startPointer = clientToNormalized(event);
  dragState.startCenter = { ...surface.center };
  dragState.startRadiusX = surface.radiusX;
  dragState.startRadiusY = surface.radiusY;
  dragState.pointerId = event.pointerId;
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

// Start dragging ellipse radius-Y handle
const startEllipseRadiusYDrag = (surface: EllipseSurface, event: PointerEvent) => {
  if (event.button !== 0) return;
  if (spacebarHeld.value) {
    startPan(event);
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  suppressClear.value = true;
  dragState.mode = "ellipse-radius-y";
  dragState.surfaceId = surface.id;
  dragState.startPointer = clientToNormalized(event);
  dragState.startCenter = { ...surface.center };
  dragState.startRadiusX = surface.radiusX;
  dragState.startRadiusY = surface.radiusY;
  dragState.pointerId = event.pointerId;
  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

const startRotationDrag = (surface: Surface, event: PointerEvent) => {
  if (event.button !== 0) {
    return;
  }
  // If spacebar is held, pan instead of rotating
  if (spacebarHeld.value) {
    startPan(event);
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  suppressClear.value = true;

  const center = getSurfaceCenterNorm(surface);
  const pointerPos = clientToNormalized(event);
  const startAngle = Math.atan2(pointerPos.y - center.y, pointerPos.x - center.x);

  // Get vertices safely for polygon surfaces
  const vertices = isEllipseSurface(surface) ? [] : ("vertices" in surface ? surface.vertices : []);

  dragState.mode = "rotation";
  dragState.surfaceId = surface.id;
  dragState.vertexIndex = null;
  dragState.startPointer = pointerPos;
  dragState.startVertices = vertices.map((vertex) => ({ ...vertex }));
  dragState.startRotation = surface.rotation;
  dragState.startAngle = startAngle;
  dragState.startCenter = isEllipseSurface(surface) ? { ...surface.center } : null;
  dragState.pointerId = event.pointerId;

  sceneStore.setActiveSurfaceId(surface.id);
  viewportRef.value?.setPointerCapture(event.pointerId);
};

const moveDrag = (event: PointerEvent) => {
  if (!dragState.mode || !dragState.surfaceId || !dragState.startPointer) {
    return;
  }

  const surface = findSurface(dragState.surfaceId);
  if (!surface) return;

  // Handle ellipse-specific drag modes
  if (isEllipseSurface(surface)) {
    const current = clientToNormalized(event);
    const delta = {
      x: current.x - dragState.startPointer.x,
      y: current.y - dragState.startPointer.y,
    };

    if (dragState.mode === "ellipse-center" && dragState.startCenter) {
      // Move ellipse center (no clamping - allow shapes to extend beyond viewport)
      surface.center = {
        x: dragState.startCenter.x + delta.x,
        y: dragState.startCenter.y + delta.y,
      };
      return;
    }

    if (dragState.mode === "ellipse-radius-x" && dragState.startCenter) {
      // Adjust horizontal radius based on distance from center
      const newRadiusX = Math.abs(current.x - dragState.startCenter.x);
      // Shift: keep aspect ratio (scale both radii proportionally)
      if (event.shiftKey) {
        const scale = newRadiusX / dragState.startRadiusX;
        surface.radiusX = Math.max(newRadiusX, 0.02);
        surface.radiusY = Math.max(dragState.startRadiusY * scale, 0.02);
      } else {
        surface.radiusX = Math.max(newRadiusX, 0.02);
      }
      return;
    }

    if (dragState.mode === "ellipse-radius-y" && dragState.startCenter) {
      // Adjust vertical radius based on distance from center
      const newRadiusY = Math.abs(current.y - dragState.startCenter.y);
      // Shift: keep aspect ratio (scale both radii proportionally)
      if (event.shiftKey) {
        const scale = newRadiusY / dragState.startRadiusY;
        surface.radiusY = Math.max(newRadiusY, 0.02);
        surface.radiusX = Math.max(dragState.startRadiusX * scale, 0.02);
      } else {
        surface.radiusY = Math.max(newRadiusY, 0.02);
      }
      return;
    }

    if (dragState.mode === "rotation" && dragState.startCenter) {
      // Ellipse rotation
      const pointerPos = clientToNormalized(event);
      const currentAngle = Math.atan2(pointerPos.y - dragState.startCenter.y, pointerPos.x - dragState.startCenter.x);
      const angleDelta = (currentAngle - dragState.startAngle) * (180 / Math.PI);
      surface.rotation = dragState.startRotation + angleDelta;
      return;
    }
  }

  if (dragState.mode === "rotation") {
    // Calculate rotation based on angle change from center (polygon surfaces)
    const center = getSurfaceCenterNorm({ vertices: dragState.startVertices } as Surface);
    const pointerPos = clientToNormalized(event);
    const currentAngle = Math.atan2(pointerPos.y - center.y, pointerPos.x - center.x);
    // Dragging clockwise should rotate content clockwise
    const angleDelta = (currentAngle - dragState.startAngle) * (180 / Math.PI);

    // Update surface rotation value
    surface.rotation = dragState.startRotation + angleDelta;
    return;
  }

  const current = clientToNormalized(event);
  let delta = {
    x: current.x - dragState.startPointer.x,
    y: current.y - dragState.startPointer.y,
  };

  // Shift: Constrain proportions (equal movement in X and Y for uniform scaling feel)
  if (event.shiftKey && dragState.mode === "vertex") {
    const maxDelta = Math.max(Math.abs(delta.x), Math.abs(delta.y));
    delta = {
      x: Math.sign(delta.x) * maxDelta,
      y: Math.sign(delta.y) * maxDelta,
    };
  }

  // Alt: Scale from center (move vertex and its opposite symmetrically)
  const scaleFromCenter = event.altKey && dragState.mode === "vertex" && dragState.vertexIndex !== null;

  // Calculate center of original shape for center-scaling
  const originalCenter = scaleFromCenter ? {
    x: dragState.startVertices.reduce((sum, v) => sum + v.x, 0) / dragState.startVertices.length,
    y: dragState.startVertices.reduce((sum, v) => sum + v.y, 0) / dragState.startVertices.length,
  } : null;

  const nextVertices = dragState.startVertices.map((vertex, index) => {
    if (dragState.mode === "vertex") {
      if (scaleFromCenter && originalCenter) {
        // Scale all vertices from center based on the dragged vertex movement
        const draggedVertex = dragState.startVertices[dragState.vertexIndex!];
        const originalDistFromCenter = {
          x: draggedVertex.x - originalCenter.x,
          y: draggedVertex.y - originalCenter.y,
        };
        // Calculate scale factor based on new position relative to center
        const newDraggedPos = {
          x: draggedVertex.x + delta.x,
          y: draggedVertex.y + delta.y,
        };
        const newDistFromCenter = {
          x: newDraggedPos.x - originalCenter.x,
          y: newDraggedPos.y - originalCenter.y,
        };
        // Avoid division by zero
        const scaleX = Math.abs(originalDistFromCenter.x) > 0.001
          ? newDistFromCenter.x / originalDistFromCenter.x
          : 1;
        const scaleY = Math.abs(originalDistFromCenter.y) > 0.001
          ? newDistFromCenter.y / originalDistFromCenter.y
          : 1;
        // Use uniform scale if Shift is also held
        const scale = event.shiftKey ? Math.max(scaleX, scaleY) : { x: scaleX, y: scaleY };
        const uniformScale = typeof scale === "number";

        // No clamping - allow shapes to extend beyond viewport
        return {
          x: originalCenter.x + (vertex.x - originalCenter.x) * (uniformScale ? scale : scale.x),
          y: originalCenter.y + (vertex.y - originalCenter.y) * (uniformScale ? scale : scale.y),
        };
      } else if (dragState.vertexIndex !== index) {
        return vertex;
      }
    }
    // No clamping - allow shapes to extend beyond viewport
    const newVertex = {
      x: vertex.x + delta.x,
      y: vertex.y + delta.y,
    };
    // Apply snapping only for single vertex drags (not when scaling from center)
    if (dragState.mode === "vertex" && !scaleFromCenter) {
      return snapVertex(newVertex, dragState.surfaceId!);
    }
    return newVertex;
  });

  updateSurfaceVertices(dragState.surfaceId, nextVertices);

  // Send crosshair position to renderer during vertex drag
  if (dragState.mode === "vertex" && dragState.vertexIndex !== null) {
    const vertexPos = nextVertices[dragState.vertexIndex];
    if (vertexPos) {
      void rendererStore.showCrosshair(true, vertexPos.x, vertexPos.y);
    }
  }
};

const endDrag = async () => {
  // Hide crosshair on renderer when vertex drag ends
  const wasVertexDrag = dragState.mode === "vertex";
  const wasEllipseDrag = dragState.mode?.startsWith("ellipse-");
  if (wasVertexDrag) {
    void rendererStore.showCrosshair(false);
  }

  // Release pointer capture to prevent "stuck" drag behavior
  if (dragState.pointerId !== null && viewportRef.value) {
    try {
      viewportRef.value.releasePointerCapture(dragState.pointerId);
    } catch {
      // Ignore if pointer capture was already released
    }
  }

  if (dragState.mode && activeScene.value) {
    await sceneStore.updateScene({ ...activeScene.value });
  }
  dragState.mode = null;

  // After vertex/ellipse drag, send scene to renderer immediately if live preview is on
  // (we skip updates during drag to avoid video flashing)
  if ((wasVertexDrag || wasEllipseDrag) && livePreview.value && canPreview.value) {
    void previewScene();
  }
  dragState.surfaceId = null;
  dragState.vertexIndex = null;
  dragState.startPointer = null;
  dragState.startVertices = [];
  dragState.startRotation = 0;
  dragState.startAngle = 0;
  dragState.startCenter = null;
  dragState.startRadiusX = 0;
  dragState.startRadiusY = 0;
  dragState.pointerId = null;
};

const clearSelection = () => {
  // Always close shape menu on any click in viewport
  showShapeMenu.value = false;

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

// Get polygon points for any surface (ellipse uses generated vertices)
const surfacePoints = (surface: Surface) => {
  const vertices = getSurfaceVertices(surface);
  return vertices.map((vertex) => toPoint(vertex.x, vertex.y)).join(" ");
};

// Compute center of the active surface for center point indicator
const surfaceCenter = computed(() => {
  if (!activeSurface.value) {
    return null;
  }
  if (isEllipseSurface(activeSurface.value)) {
    return toStage(activeSurface.value.center.x, activeSurface.value.center.y);
  }
  const vertices = activeSurface.value.vertices;
  const sumX = vertices.reduce((acc, v) => acc + v.x, 0);
  const sumY = vertices.reduce((acc, v) => acc + v.y, 0);
  const centerNorm = {
    x: sumX / vertices.length,
    y: sumY / vertices.length,
  };
  return toStage(centerNorm.x, centerNorm.y);
});

// Ellipse radius handles (for ellipse surfaces)
const ellipseRadiusHandles = computed(() => {
  if (!activeSurface.value || !isEllipseSurface(activeSurface.value)) {
    return null;
  }
  const ellipse = activeSurface.value;
  const center = toStage(ellipse.center.x, ellipse.center.y);
  // Convert radii from normalized (-1 to 1) to stage coordinates
  const radiusXStage = ellipse.radiusX * stageWidth * 0.5;
  const radiusYStage = ellipse.radiusY * stageHeight * 0.5;
  return {
    center,
    radiusXHandle: { x: center.x + radiusXStage, y: center.y },
    radiusYHandle: { x: center.x, y: center.y + radiusYStage },
    radiusXStage,
    radiusYStage,
  };
});

// Compute rotation handle position (extends from center)
const rotationHandleOffset = 90; // Distance from center in stage pixels
const rotationHandle = computed(() => {
  if (!activeSurface.value || !surfaceCenter.value) {
    return null;
  }
  // Handle extends upward from center, rotated by current rotation
  const angleRad = (activeSurface.value.rotation * Math.PI) / 180;
  // Rotate from "up" (-90 degrees) by the surface rotation
  const handleAngle = -Math.PI / 2 + angleRad;
  return {
    x: surfaceCenter.value.x + Math.cos(handleAngle) * rotationHandleOffset,
    y: surfaceCenter.value.y + Math.sin(handleAngle) * rotationHandleOffset,
  };
});

// Compute the dragged vertex position for the crosshair indicator
const draggedVertexPosition = computed(() => {
  if (dragState.mode !== "vertex" || dragState.vertexIndex === null || !dragState.surfaceId) {
    return null;
  }
  const surface = findSurface(dragState.surfaceId);
  if (!surface || isEllipseSurface(surface)) {
    return null;
  }
  const vertices = "vertices" in surface ? surface.vertices : [];
  if (!vertices || dragState.vertexIndex >= vertices.length) {
    return null;
  }
  const vertex = vertices[dragState.vertexIndex];
  return toStage(vertex.x, vertex.y);
});

// Delete the active surface
const deleteSurface = async () => {
  if (!activeScene.value || !activeSurfaceId.value) {
    return;
  }
  const nextSurfaces = activeScene.value.surfaces.filter(
    (s) => s.id !== activeSurfaceId.value
  );
  const nextScene = {
    ...activeScene.value,
    surfaces: nextSurfaces,
  };
  await sceneStore.updateScene(nextScene);
  sceneStore.setActiveSurfaceId(null);
};

// Duplicate the active surface
const duplicateSurface = async () => {
  if (!activeScene.value || !activeSurface.value) {
    return;
  }
  const offset = 0.05; // Small offset so it's visible

  let newSurface: Surface;

  if (isEllipseSurface(activeSurface.value)) {
    // Duplicate ellipse surface
    newSurface = createSurface("ellipse", {
      feedId: activeSurface.value.feedId,
      zOrder: activeScene.value.surfaces.length,
      index: activeScene.value.surfaces.length + 1,
    }) as EllipseSurface;
    // Copy ellipse-specific properties
    (newSurface as EllipseSurface).center = {
      x: activeSurface.value.center.x + offset,
      y: activeSurface.value.center.y + offset,
    };
    (newSurface as EllipseSurface).radiusX = activeSurface.value.radiusX;
    (newSurface as EllipseSurface).radiusY = activeSurface.value.radiusY;
  } else {
    // Duplicate polygon surface
    newSurface = createSurface("quad", {
      feedId: activeSurface.value.feedId,
      zOrder: activeScene.value.surfaces.length,
      index: activeScene.value.surfaces.length + 1,
    });
    // Copy vertices with offset
    if ("vertices" in newSurface) {
      newSurface.vertices = activeSurface.value.vertices.map((v) => ({
        x: v.x + offset,
        y: v.y + offset,
      }));
    }
  }

  newSurface.name = `${activeSurface.value.name} Copy`;
  newSurface.opacity = activeSurface.value.opacity;
  newSurface.brightness = activeSurface.value.brightness;
  newSurface.blendMode = activeSurface.value.blendMode;
  newSurface.rotation = activeSurface.value.rotation;

  const nextScene = {
    ...activeScene.value,
    surfaces: [...activeScene.value.surfaces, newSurface],
  };
  await sceneStore.updateScene(nextScene);
  sceneStore.setActiveSurfaceId(newSurface.id);
};

</script>

<template>
  <div class="scene-canvas">
    <div class="scene-canvas__header">
      <div class="scene-canvas__title">Output Preview</div>
      <div class="scene-canvas__controls">
        <label
          v-if="hasActiveScene"
          class="scene-canvas__toggle"
          :class="{ 'scene-canvas__toggle--active': livePreview, 'scene-canvas__toggle--disabled': !canPreview }"
          title="Live preview - sync scene to renderer in real-time (Ctrl/Cmd+P)"
        >
          <input type="checkbox" v-model="livePreview" :disabled="!canPreview" />
          <span class="scene-canvas__toggle-track">
            <span class="scene-canvas__toggle-thumb"></span>
          </span>
          <span class="scene-canvas__toggle-label">Live Preview</span>
        </label>
        <label
          class="scene-canvas__toggle scene-canvas__toggle--cyan"
          :class="{ 'scene-canvas__toggle--active': showProjectorGrid }"
          title="Show projector calibration grid"
        >
          <input type="checkbox" v-model="showProjectorGrid" />
          <span class="scene-canvas__toggle-track">
            <span class="scene-canvas__toggle-thumb"></span>
          </span>
          <span class="scene-canvas__toggle-label">Grid</span>
        </label>
        <label
          v-if="hasActiveScene"
          class="scene-canvas__toggle scene-canvas__toggle--cyan"
          :class="{ 'scene-canvas__toggle--active': snapEnabled }"
          title="Toggle vertex and edge snapping"
        >
          <input type="checkbox" v-model="snapEnabled" />
          <span class="scene-canvas__toggle-track">
            <span class="scene-canvas__toggle-thumb"></span>
          </span>
          <span class="scene-canvas__toggle-label">Snap</span>
        </label>
        <div class="scene-canvas__zoom-controls">
          <span class="scene-canvas__zoom-value" data-testid="zoom-label" @click="zoom = 1" title="Click to reset to 100%">{{ zoomLabel }}</span>
          <input
            type="range"
            class="scene-canvas__zoom-slider"
            :value="zoom"
            min="0.1"
            max="2"
            step="0.05"
            title="Drag to zoom (scroll wheel also works)"
            @input="zoom = parseFloat(($event.target as HTMLInputElement).value)"
          />
          <Button
            icon="pi pi-expand"
            label="Fit"
            text
            size="small"
            class="scene-canvas__zoom-btn"
            title="Fit to view (F)"
            @click="fitToView"
          />
        </div>
      </div>
    </div>

    <div v-if="!hasActiveScene" class="scene-canvas__empty">
      <i class="pi pi-images scene-canvas__empty-icon"></i>
      <p>No scene selected</p>
      <span class="scene-canvas__empty-hint">Select or create a scene from the Browser panel to start mapping surfaces.</span>
    </div>

    <div v-else class="scene-canvas__workspace">
      <div
        ref="viewportRef"
        class="scene-canvas__viewport"
        :class="{ 'scene-canvas__viewport--pan-mode': spacebarHeld }"
        data-testid="scene-viewport"
        :title="spacebarHeld ? 'Pan mode: drag to pan the viewport' : 'Left-click to select surface, drag to move, scroll to zoom. Hold Space + drag to pan'"
        @wheel.prevent="onWheel"
        @pointerdown="startPan"
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
            overflow="visible"
            role="presentation"
          >
            <!-- Output boundary frame -->
            <rect
              class="scene-canvas__boundary"
              x="0"
              y="0"
              :width="stageWidth"
              :height="stageHeight"
            />
            <!-- Corner markers -->
            <g class="scene-canvas__boundary-corners">
              <!-- Top-left -->
              <path d="M 0 40 L 0 0 L 40 0" />
              <!-- Top-right -->
              <path :d="`M ${stageWidth - 40} 0 L ${stageWidth} 0 L ${stageWidth} 40`" />
              <!-- Bottom-left -->
              <path :d="`M 0 ${stageHeight - 40} L 0 ${stageHeight} L 40 ${stageHeight}`" />
              <!-- Bottom-right -->
              <path :d="`M ${stageWidth - 40} ${stageHeight} L ${stageWidth} ${stageHeight} L ${stageWidth} ${stageHeight - 40}`" />
            </g>
            <!-- Center crosshair -->
            <g class="scene-canvas__boundary-center">
              <line :x1="stageWidth / 2 - 30" :y1="stageHeight / 2" :x2="stageWidth / 2 + 30" :y2="stageHeight / 2" />
              <line :x1="stageWidth / 2" :y1="stageHeight / 2 - 30" :x2="stageWidth / 2" :y2="stageHeight / 2 + 30" />
              <circle :cx="stageWidth / 2" :cy="stageHeight / 2" r="8" />
            </g>

            <!-- Projector calibration grid overlay -->
            <g v-if="showProjectorGrid" class="scene-canvas__projector-grid">
              <!-- Main boundary -->
              <rect
                class="scene-canvas__projector-boundary"
                x="1"
                y="1"
                :width="stageWidth - 2"
                :height="stageHeight - 2"
              />
              <!-- Vertical grid lines -->
              <line
                v-for="i in 15"
                :key="`v-${i}`"
                class="scene-canvas__projector-line"
                :x1="(stageWidth / 16) * i"
                y1="0"
                :x2="(stageWidth / 16) * i"
                :y2="stageHeight"
              />
              <!-- Horizontal grid lines -->
              <line
                v-for="i in 8"
                :key="`h-${i}`"
                class="scene-canvas__projector-line"
                x1="0"
                :y1="(stageHeight / 9) * i"
                :x2="stageWidth"
                :y2="(stageHeight / 9) * i"
              />
              <!-- Center crosshair -->
              <line
                class="scene-canvas__projector-center"
                :x1="stageWidth / 2"
                y1="0"
                :x2="stageWidth / 2"
                :y2="stageHeight"
              />
              <line
                class="scene-canvas__projector-center"
                x1="0"
                :y1="stageHeight / 2"
                :x2="stageWidth"
                :y2="stageHeight / 2"
              />
              <!-- Diagonal lines for alignment -->
              <line
                class="scene-canvas__projector-diagonal"
                x1="0"
                y1="0"
                :x2="stageWidth"
                :y2="stageHeight"
              />
              <line
                class="scene-canvas__projector-diagonal"
                :x1="stageWidth"
                y1="0"
                x2="0"
                :y2="stageHeight"
              />
              <!-- Resolution label -->
              <text
                class="scene-canvas__projector-label"
                :x="stageWidth / 2"
                y="40"
                text-anchor="middle"
              >CALIBRATION GRID • 1920×1080</text>
            </g>

            <polygon
              v-for="surface in activeScene?.surfaces ?? []"
              :key="surface.id"
              :points="surfacePoints(surface)"
              :data-surface-id="surface.id"
              class="scene-canvas__surface"
              :class="{ 'scene-canvas__surface--active': surface.id === activeSurfaceId }"
              @click.stop="selectSurface(surface.id)"
              @pointerdown.stop="startShapeDrag(surface, $event)"
            >
              <title>{{ surface.name || 'Surface' }} - Click to select, drag to move</title>
            </polygon>

            <!-- Surface labels (shown at center of each surface) -->
            <text
              v-for="surface in activeScene?.surfaces ?? []"
              :key="`label-${surface.id}`"
              class="scene-canvas__surface-label"
              :class="{ 'scene-canvas__surface-label--active': surface.id === activeSurfaceId }"
              :x="getSurfaceLabelInfo(surface)?.x"
              :y="getSurfaceLabelInfo(surface)?.y"
              text-anchor="middle"
              dominant-baseline="central"
              @click.stop="selectSurface(surface.id)"
              @pointerdown.stop="startShapeDrag(surface, $event)"
            >{{ getSurfaceLabelInfo(surface)?.name }}</text>

            <!-- Polygon vertex handles (only for non-ellipse surfaces) -->
            <circle
              v-for="(vertex, index) in (activeSurface && !isEllipseSurface(activeSurface)) ? activeSurface.vertices : []"
              :key="`handle-${index}`"
              class="scene-canvas__handle"
              :cx="toStage(vertex.x, vertex.y).x"
              :cy="toStage(vertex.x, vertex.y).y"
              r="18"
              @pointerdown.stop="startVertexDrag(activeSurface!, index, $event)"
              @click.stop
            >
              <title>Corner {{ index + 1 }} - Drag to adjust</title>
            </circle>

            <!-- Ellipse radius handles -->
            <g v-if="activeSurface && isEllipseSurface(activeSurface) && ellipseRadiusHandles">
              <!-- Horizontal radius line -->
              <line
                class="scene-canvas__ellipse-radius-line"
                :x1="ellipseRadiusHandles.center.x"
                :y1="ellipseRadiusHandles.center.y"
                :x2="ellipseRadiusHandles.radiusXHandle.x"
                :y2="ellipseRadiusHandles.radiusXHandle.y"
              />
              <!-- Vertical radius line -->
              <line
                class="scene-canvas__ellipse-radius-line"
                :x1="ellipseRadiusHandles.center.x"
                :y1="ellipseRadiusHandles.center.y"
                :x2="ellipseRadiusHandles.radiusYHandle.x"
                :y2="ellipseRadiusHandles.radiusYHandle.y"
              />
              <!-- Horizontal radius handle (radiusX) -->
              <circle
                class="scene-canvas__ellipse-handle scene-canvas__ellipse-handle--x"
                :cx="ellipseRadiusHandles.radiusXHandle.x"
                :cy="ellipseRadiusHandles.radiusXHandle.y"
                r="14"
                @pointerdown.stop="startEllipseRadiusXDrag(activeSurface as EllipseSurface, $event)"
                @click.stop
              >
                <title>Horizontal radius - Drag to resize (Shift to constrain)</title>
              </circle>
              <!-- Vertical radius handle (radiusY) -->
              <circle
                class="scene-canvas__ellipse-handle scene-canvas__ellipse-handle--y"
                :cx="ellipseRadiusHandles.radiusYHandle.x"
                :cy="ellipseRadiusHandles.radiusYHandle.y"
                r="14"
                @pointerdown.stop="startEllipseRadiusYDrag(activeSurface as EllipseSurface, $event)"
                @click.stop
              >
                <title>Vertical radius - Drag to resize (Shift to constrain)</title>
              </circle>
            </g>

            <!-- Rotation handle for selected surface -->
            <g v-if="activeSurface && surfaceCenter && rotationHandle">
              <!-- Line from center to rotation handle -->
              <line
                class="scene-canvas__rotation-line"
                :x1="surfaceCenter.x"
                :y1="surfaceCenter.y"
                :x2="rotationHandle.x"
                :y2="rotationHandle.y"
              />
              <!-- Center point indicator -->
              <circle
                class="scene-canvas__center-point"
                :cx="surfaceCenter.x"
                :cy="surfaceCenter.y"
                r="5"
              />
              <!-- Rotation handle (draggable) -->
              <circle
                class="scene-canvas__rotation-handle"
                :cx="rotationHandle.x"
                :cy="rotationHandle.y"
                r="12"
                @pointerdown.stop="startRotationDrag(activeSurface, $event)"
                @click.stop
              >
                <title>Drag to rotate ({{ Math.round(activeSurface.rotation) }}°)</title>
              </circle>
              <!-- Rotation icon inside handle -->
              <text
                class="scene-canvas__rotation-icon"
                :x="rotationHandle.x"
                :y="rotationHandle.y"
                text-anchor="middle"
                dominant-baseline="central"
                @pointerdown.stop="startRotationDrag(activeSurface, $event)"
                @click.stop
              >↻</text>
            </g>

            <!-- Vertex drag indicator - crosshair showing vertex position -->
            <g v-if="draggedVertexPosition" class="scene-canvas__drag-indicator">
              <!-- Horizontal line across full stage -->
              <line
                class="scene-canvas__drag-line"
                x1="0"
                :y1="draggedVertexPosition.y"
                :x2="stageWidth"
                :y2="draggedVertexPosition.y"
              />
              <!-- Vertical line across full stage -->
              <line
                class="scene-canvas__drag-line"
                :x1="draggedVertexPosition.x"
                y1="0"
                :x2="draggedVertexPosition.x"
                :y2="stageHeight"
              />
              <!-- Center crosshair highlight -->
              <circle
                class="scene-canvas__drag-point"
                :cx="draggedVertexPosition.x"
                :cy="draggedVertexPosition.y"
                r="8"
              />
              <!-- Coordinate label -->
              <text
                class="scene-canvas__drag-coords"
                :x="draggedVertexPosition.x + 15"
                :y="draggedVertexPosition.y - 15"
                text-anchor="start"
              >{{ Math.round(draggedVertexPosition.x) }}, {{ Math.round(draggedVertexPosition.y) }}</text>
            </g>
          </svg>
        </div>

        <!-- Surface context toolbar - appears when a surface is selected -->
        <div v-if="activeSurface" class="scene-canvas__context-bar" @pointerdown.stop @click.stop>
          <span class="scene-canvas__context-name" :title="activeSurface.name">{{ activeSurface.name }}</span>
          <div class="scene-canvas__context-divider"></div>
          <Button
            icon="pi pi-copy"
            text
            size="small"
            class="scene-canvas__context-btn"
            title="Duplicate surface (Ctrl/Cmd+D)"
            @click="duplicateSurface"
          />
          <Button
            icon="pi pi-trash"
            text
            size="small"
            severity="danger"
            class="scene-canvas__context-btn scene-canvas__context-btn--danger"
            title="Delete surface (Delete)"
            @click="deleteSurface"
          />
        </div>

        <div class="scene-canvas__toolbar" @pointerdown.stop @click.stop>
          <div class="scene-canvas__add-dropdown">
            <button
              type="button"
              :disabled="!canAddShape"
              class="scene-canvas__add-btn"
              title="Add a new surface to the scene"
              @click="toggleAddMenu"
            >
              <svg class="scene-canvas__add-btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                <!-- Irregular quad shape -->
                <path d="M4 5L19 3L21 18L6 20Z" stroke-linejoin="round" />
                <!-- Vertex handles -->
                <circle cx="4" cy="5" r="2" fill="currentColor" stroke="none" />
                <circle cx="19" cy="3" r="2" fill="currentColor" stroke="none" />
                <circle cx="21" cy="18" r="2" fill="currentColor" stroke="none" />
                <circle cx="6" cy="20" r="2" fill="currentColor" stroke="none" />
              </svg>
              <span>Add Shape</span>
              <svg class="scene-canvas__add-btn-chevron" viewBox="0 0 12 12" fill="currentColor">
                <path d="M2.5 4.5L6 8L9.5 4.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
              </svg>
            </button>
            <!-- Custom dropdown menu -->
            <div v-if="showShapeMenu" class="scene-canvas__shape-menu" @click.stop>
              <button
                class="scene-canvas__shape-item"
                title="Axis-aligned rectangle"
                @click="addSurface('rectangle'); showShapeMenu = false"
              >
                <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2">
                  <rect x="2" y="3" width="12" height="10" rx="0.5" />
                </svg>
                <span>Rectangle</span>
              </button>
              <button
                class="scene-canvas__shape-item"
                title="Irregular 4-sided polygon for perspective mapping"
                @click="addSurface('quad'); showShapeMenu = false"
              >
                <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2">
                  <path d="M2 4L13 2L14 13L3 14Z" stroke-linejoin="round" />
                </svg>
                <span>Quad</span>
              </button>
              <button
                class="scene-canvas__shape-item"
                title="Circle or ellipse with adjustable radii"
                @click="addSurface('ellipse'); showShapeMenu = false"
              >
                <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2">
                  <ellipse cx="8" cy="8" rx="6" ry="5" />
                </svg>
                <span>Ellipse</span>
              </button>
            </div>
          </div>
        </div>
      </div>

      <div v-if="!hasFeeds" class="scene-canvas__hint">
        <i class="pi pi-info-circle"></i>
        Add a feed in the Browser panel to enable shape creation.
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
  gap: 8px;
  height: 100%;
  padding: 8px;
  overflow: hidden;
  position: relative;
}

.scene-canvas__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  border-bottom: 1px solid #2a2a2a;
  position: relative;
  z-index: 10;
  background: #161616;
  margin: -8px -8px 0 -8px;
  padding: 8px;
  padding-bottom: 8px;
}

.scene-canvas__title {
  font-size: 11px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #888;
}

.scene-canvas__controls {
  display: flex;
  align-items: center;
  gap: 8px;
}

.scene-canvas__meta {
  font-size: 11px;
  color: #666;
  background: #1a1a1a;
  padding: 2px 6px;
  border-radius: 2px;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', monospace;
  font-weight: 500;
}

.scene-canvas__zoom-controls {
  display: flex;
  align-items: center;
  gap: 6px;
  background: #1a1a1a;
  border-radius: 2px;
  padding: 2px 6px;
}

.scene-canvas__zoom-value {
  font-size: 11px;
  color: #888;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', monospace;
  font-weight: 500;
  min-width: 36px;
  text-align: center;
  cursor: pointer;
  transition: color 0.12s ease;
}

.scene-canvas__zoom-value:hover {
  color: #00b4d8;
}

.scene-canvas__zoom-slider {
  -webkit-appearance: none;
  appearance: none;
  width: 80px;
  height: 4px;
  background: #333;
  border-radius: 2px;
  cursor: pointer;
}

.scene-canvas__zoom-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 12px;
  height: 12px;
  background: #666;
  border-radius: 50%;
  cursor: pointer;
  transition: background 0.12s ease;
}

.scene-canvas__zoom-slider::-webkit-slider-thumb:hover {
  background: #00b4d8;
}

.scene-canvas__zoom-slider::-moz-range-thumb {
  width: 12px;
  height: 12px;
  background: #666;
  border: none;
  border-radius: 50%;
  cursor: pointer;
  transition: background 0.12s ease;
}

.scene-canvas__zoom-slider::-moz-range-thumb:hover {
  background: #00b4d8;
}

.scene-canvas__zoom-btn {
  padding: 2px 6px !important;
  min-width: 0 !important;
  font-size: 10px !important;
  gap: 4px !important;
}

.scene-canvas__zoom-btn :deep(.p-button-icon) {
  font-size: 10px;
}

.scene-canvas__zoom-btn :deep(.p-button-label) {
  font-size: 10px;
  font-weight: 500;
}

/* Classic toggle switch */
.scene-canvas__toggle {
  display: flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
  user-select: none;
}

.scene-canvas__toggle input {
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.scene-canvas__toggle-track {
  position: relative;
  width: 28px;
  height: 16px;
  background: #333;
  border-radius: 8px;
  transition: all 0.2s ease;
  border: 1px solid #444;
}

.scene-canvas__toggle-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 10px;
  height: 10px;
  background: #666;
  border-radius: 50%;
  transition: all 0.2s ease;
}

.scene-canvas__toggle-label {
  font-size: 11px;
  color: #666;
  font-weight: 500;
  transition: color 0.2s ease;
}

.scene-canvas__toggle:hover .scene-canvas__toggle-track {
  border-color: #555;
}

.scene-canvas__toggle:hover .scene-canvas__toggle-thumb {
  background: #888;
}

.scene-canvas__toggle:hover .scene-canvas__toggle-label {
  color: #888;
}

/* Active state - default red accent (for Live toggle) */
.scene-canvas__toggle--active .scene-canvas__toggle-track {
  background: rgba(255, 59, 48, 0.3);
  border-color: rgba(255, 59, 48, 0.5);
}

.scene-canvas__toggle--active .scene-canvas__toggle-thumb {
  left: 14px;
  background: #ff3b30;
}

.scene-canvas__toggle--active .scene-canvas__toggle-label {
  color: #ff3b30;
}

/* Cyan variant (for Grid, Snap toggles) */
.scene-canvas__toggle--cyan.scene-canvas__toggle--active .scene-canvas__toggle-track {
  background: rgba(0, 180, 216, 0.3);
  border-color: rgba(0, 180, 216, 0.5);
}

.scene-canvas__toggle--cyan.scene-canvas__toggle--active .scene-canvas__toggle-thumb {
  background: #00b4d8;
}

.scene-canvas__toggle--cyan.scene-canvas__toggle--active .scene-canvas__toggle-label {
  color: #00b4d8;
}

/* Disabled state */
.scene-canvas__toggle--disabled {
  cursor: not-allowed;
  opacity: 0.4;
}

.scene-canvas__toggle--disabled:hover .scene-canvas__toggle-track {
  border-color: #444;
}

.scene-canvas__toggle--disabled:hover .scene-canvas__toggle-thumb {
  background: #666;
}

.scene-canvas__toggle--disabled:hover .scene-canvas__toggle-label {
  color: #666;
}

.scene-canvas__empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 32px 16px;
  color: #555;
  flex: 1;
}

.scene-canvas__empty-icon {
  font-size: 2rem;
  color: rgba(0, 180, 216, 0.2);
  margin-bottom: 10px;
}

.scene-canvas__empty p {
  margin: 0 0 6px;
  font-size: 12px;
  font-weight: 500;
  color: #777;
}

.scene-canvas__empty-hint {
  font-size: 11px;
  color: #555;
  max-width: 240px;
  line-height: 1.4;
}

.scene-canvas__workspace {
  display: flex;
  flex-direction: column;
  gap: 8px;
  flex: 1;
  overflow: hidden;
  position: relative;
}

.scene-canvas__viewport {
  position: relative;
  width: 100%;
  flex: 1;
  min-height: 200px;
  margin: 0 auto;
  border-radius: 0;
  background: #0a0a0a;
  overflow: visible;
  touch-action: none;
  z-index: 1;
}

/* Border overlay - renders on top of overflowing content */
.scene-canvas__viewport::after {
  content: '';
  position: absolute;
  inset: 0;
  border: 1px solid #2a2a2a;
  pointer-events: none;
  z-index: 100;
}

/* Pan mode cursor when spacebar is held */
.scene-canvas__viewport--pan-mode {
  cursor: grab !important;
}

.scene-canvas__viewport--pan-mode:active {
  cursor: grabbing !important;
}

/* Override all child element cursors in pan mode */
.scene-canvas__viewport--pan-mode * {
  cursor: inherit !important;
}

.scene-canvas__stage {
  position: relative;
  width: 1920px;
  height: 1080px;
  transform-origin: 0 0;
  background:
    linear-gradient(90deg, #1a1a1a 1px, transparent 1px) 0 0 / 48px 48px,
    linear-gradient(#1a1a1a 1px, transparent 1px) 0 0 / 48px 48px,
    #0a0a0a;
}

.scene-canvas__surfaces {
  position: absolute;
  inset: 0;
}

/* Output boundary frame */
.scene-canvas__boundary {
  fill: none;
  stroke: rgba(0, 180, 216, 0.3);
  stroke-width: 2;
  stroke-dasharray: 8 4;
}

.scene-canvas__boundary-corners path {
  fill: none;
  stroke: #00b4d8;
  stroke-width: 3;
  stroke-linecap: square;
}

.scene-canvas__boundary-center line {
  stroke: rgba(255, 149, 0, 0.5);
  stroke-width: 1.5;
  stroke-dasharray: 4 3;
}

.scene-canvas__boundary-center circle {
  fill: none;
  stroke: rgba(255, 149, 0, 0.5);
  stroke-width: 1.5;
}

.scene-canvas__surface {
  fill: rgba(0, 180, 216, 0.08);
  stroke: rgba(0, 180, 216, 0.5);
  stroke-width: 2;
  cursor: pointer;
  transition: fill 0.15s ease, stroke 0.15s ease;
}

.scene-canvas__surface:hover {
  fill: rgba(0, 180, 216, 0.12);
  stroke: rgba(0, 180, 216, 0.7);
}

.scene-canvas__surface--active {
  stroke: #00b4d8;
  stroke-width: 2.5;
  fill: rgba(0, 180, 216, 0.2);
}

/* Surface labels */
.scene-canvas__surface-label {
  fill: rgba(180, 180, 180, 0.6);
  font-size: 42px;
  font-weight: 500;
  pointer-events: all;
  cursor: pointer;
  user-select: none;
  text-transform: uppercase;
  letter-spacing: 2px;
  transition: fill 0.15s ease;
}

.scene-canvas__surface-label:hover {
  fill: rgba(200, 200, 200, 0.8);
}

.scene-canvas__surface-label--active {
  fill: rgba(0, 180, 216, 0.7);
}

.scene-canvas__handle {
  fill: #1a1a1a;
  stroke: #00b4d8;
  stroke-width: 3;
  cursor: grab;
  transition: fill 0.15s ease;
  filter: drop-shadow(0 0 4px rgba(0, 180, 216, 0.5));
}

.scene-canvas__handle:hover {
  fill: #00b4d8;
  stroke: #fff;
  filter: drop-shadow(0 0 8px rgba(0, 180, 216, 0.8));
}

/* Center point indicator */
.scene-canvas__center-point {
  fill: #ff9500;
  stroke: #1a1a1a;
  stroke-width: 2;
  opacity: 0.8;
}

/* Rotation handle */
.scene-canvas__rotation-line {
  stroke: rgba(255, 149, 0, 0.5);
  stroke-width: 2;
  stroke-dasharray: 4 2;
}

.scene-canvas__rotation-handle {
  fill: #ff9500;
  stroke: #1a1a1a;
  stroke-width: 2;
  cursor: grab;
  transition: fill 0.15s ease, filter 0.15s ease;
}

.scene-canvas__rotation-handle:hover {
  fill: #ffaa33;
  filter: drop-shadow(0 0 8px rgba(255, 149, 0, 0.6));
}

.scene-canvas__rotation-handle:active {
  cursor: grabbing;
}

.scene-canvas__rotation-icon {
  fill: #1a1a1a;
  font-size: 14px;
  font-weight: bold;
  pointer-events: none;
  user-select: none;
}

/* Projector calibration grid */
.scene-canvas__projector-grid {
  pointer-events: none;
}

.scene-canvas__projector-boundary {
  fill: none;
  stroke: #ff3b30;
  stroke-width: 4;
}

.scene-canvas__projector-line {
  stroke: rgba(255, 59, 48, 0.3);
  stroke-width: 1;
}

.scene-canvas__projector-center {
  stroke: rgba(255, 59, 48, 0.6);
  stroke-width: 2;
}

.scene-canvas__projector-diagonal {
  stroke: rgba(255, 59, 48, 0.2);
  stroke-width: 1;
  stroke-dasharray: 8 4;
}

.scene-canvas__projector-label {
  fill: #ff3b30;
  font-size: 28px;
  font-weight: 600;
  letter-spacing: 0.1em;
}

/* Vertex drag indicator crosshair */
.scene-canvas__drag-indicator {
  pointer-events: none;
}

.scene-canvas__drag-line {
  stroke: rgba(0, 180, 216, 0.6);
  stroke-width: 1;
  stroke-dasharray: 6 3;
}

.scene-canvas__drag-point {
  fill: none;
  stroke: #00b4d8;
  stroke-width: 2;
}

.scene-canvas__drag-coords {
  fill: #00b4d8;
  font-size: 14px;
  font-weight: 500;
  font-family: monospace;
  text-shadow: 0 1px 2px rgba(0, 0, 0, 0.8);
}

/* Fit to view button */
.scene-canvas__fit-btn {
  color: #666;
  padding: 4px 8px;
}

.scene-canvas__fit-btn:hover {
  color: #00b4d8;
  background: rgba(0, 180, 216, 0.1);
}

/* Context bar - appears when surface is selected */
.scene-canvas__context-bar {
  position: absolute;
  bottom: 8px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  align-items: center;
  gap: 4px;
  background: rgba(26, 26, 26, 0.95);
  border: 1px solid #333;
  border-radius: 3px;
  padding: 4px 8px;
  backdrop-filter: blur(8px);
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.4);
}

.scene-canvas__context-name {
  font-size: 11px;
  font-weight: 500;
  color: #aaa;
  max-width: 120px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.scene-canvas__context-divider {
  width: 1px;
  height: 16px;
  background: #333;
  margin: 0 4px;
}

.scene-canvas__context-btn {
  width: 24px;
  height: 24px;
  padding: 0;
  border-radius: 2px;
}

.scene-canvas__context-btn :deep(.p-button-icon) {
  font-size: 12px;
}

.scene-canvas__context-btn:hover {
  background: rgba(255, 255, 255, 0.08);
}

.scene-canvas__context-btn--danger:hover {
  background: rgba(180, 60, 60, 0.15);
}

.scene-canvas__toolbar {
  position: absolute;
  top: 8px;
  right: 8px;
  z-index: 10;
  display: flex;
  align-items: center;
  gap: 6px;
  background: rgba(10, 10, 10, 0.9);
  padding: 4px;
  border-radius: 4px;
}

.scene-canvas__add-btn {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  background: rgba(0, 180, 216, 0.15);
  border: 1px solid rgba(0, 180, 216, 0.4);
  color: #00b4d8;
  font-size: 11px;
  font-weight: 500;
  padding: 5px 8px;
  border-radius: 2px;
  transition: all 0.12s ease;
  cursor: pointer;
  font-family: inherit;
}

.scene-canvas__add-btn:hover:not(:disabled) {
  background: rgba(0, 180, 216, 0.25);
  border-color: rgba(0, 180, 216, 0.6);
  color: #00d4ff;
}

.scene-canvas__add-btn:disabled {
  background: rgba(60, 60, 60, 0.3);
  border-color: #444;
  color: #555;
  cursor: not-allowed;
}

.scene-canvas__add-btn-icon {
  width: 14px;
  height: 14px;
  flex-shrink: 0;
}

.scene-canvas__add-btn-chevron {
  width: 8px;
  height: 8px;
  opacity: 0.6;
  flex-shrink: 0;
  margin-left: -1px;
}

.scene-canvas__add-dropdown {
  position: relative;
}

.scene-canvas__shape-menu {
  position: absolute;
  top: 100%;
  left: 0;
  margin-top: 4px;
  background: #1a1a1a;
  border: 1px solid #2a2a2a;
  border-radius: 4px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
  padding: 4px;
  z-index: 100;
  min-width: 90px;
}

.scene-canvas__shape-item {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
  padding: 6px 8px;
  background: transparent;
  border: none;
  border-radius: 3px;
  color: #888;
  font-size: 11px;
  font-weight: 400;
  font-family: inherit;
  cursor: pointer;
  transition: all 0.1s ease;
  text-align: left;
}

.scene-canvas__shape-item:hover {
  background: rgba(0, 180, 216, 0.1);
  color: #00b4d8;
}

.scene-canvas__shape-item svg {
  width: 14px;
  height: 14px;
  flex-shrink: 0;
  opacity: 0.6;
}

.scene-canvas__shape-item:hover svg {
  opacity: 1;
  stroke: #00b4d8;
}

.scene-canvas__hint {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 11px;
  color: #777;
  padding: 7px 12px;
  background: rgba(0, 180, 216, 0.06);
  border: 1px solid rgba(0, 180, 216, 0.15);
  border-radius: 2px;
}

.scene-canvas__hint i {
  color: #00b4d8;
  font-size: 12px;
}

.scene-canvas__message {
  margin: 0;
}

/* Ellipse radius handles */
.scene-canvas__ellipse-radius-line {
  stroke: rgba(0, 180, 216, 0.4);
  stroke-width: 2;
  stroke-dasharray: 6 3;
}

.scene-canvas__ellipse-handle {
  fill: #1a1a1a;
  stroke: #00b4d8;
  stroke-width: 3;
  cursor: ew-resize;
  transition: fill 0.15s ease;
  filter: drop-shadow(0 0 4px rgba(0, 180, 216, 0.5));
}

.scene-canvas__ellipse-handle--y {
  cursor: ns-resize;
}

.scene-canvas__ellipse-handle:hover {
  fill: #00b4d8;
  stroke: #fff;
  filter: drop-shadow(0 0 8px rgba(0, 180, 216, 0.8));
}
</style>
