# Composer UI Reshape Implementation Plan

## Status
- Completed: Scene canvas component with 1920x1080 stage at 25% zoom, pan/zoom, surface selection, SpeedDial actions, and unit tests. Removed the workflow grid from the center panel.
- Completed: Left browser redesign with Scenes/Cues/Feeds/Assets/Renderers tabs, FeedBrowser CRUD UI, Scene/Cue management, RendererBrowser list + tests, and feed delete action in store.
- Completed: Asset browser upload/delete UI + client store actions, server endpoints for asset upload/delete, and HTTP tests. Updated AGENTS.md for API changes.
- Completed: Surface properties panel with feed assignment + surface fields and unit tests.
- Completed: Top-left project menu with New/Save/Close and a new project dialog.
- Completed: Scene and cue update/delete in browser with client-side safeguards and tests.
- Completed: Surface drag handles, shape drag, and surface preview action in properties panel.

## Goals
- Replace the current workflow grid with a scene canvas that displays a 1920x1080 surface at 25% zoom (480x270) with pan/zoom.
- Add shape creation via a PrimeVue SpeedDial in the canvas (top-right), and surface selection that drives the right-side properties editor.
- Introduce a top-left main menu for project management (New/Save/Close).
- Expand the left browser to cover feed CRUD, asset CRD (upload/delete), and renderer list.
- Add server APIs to support asset upload with a 2GB limit and asset deletion; expose renderer list if needed.

## Phase 0: Inventory + Constraints
1. Confirm the current Composer layout and stores (done in `clients/composer/src/views/ComposerView.vue`).
2. Identify scene/surface ownership: surfaces are stored inside `Scene.surfaces` (`clients/composer/src/types/scene.ts`).
3. Identify existing APIs: `/api/assets` (GET) and `/api/renderer/ping` (GET/POST) are already available in `server/src/http/HttpServer.cpp`.
4. Note Rule 0: if we change API/protocols, update `AGENTS.md` in the same PR.

## Phase 1: Layout + Project Menu
**Objective:** Replace the top header with a left-aligned menu and reserve the center panel for the scene canvas.

1. Update `clients/composer/src/views/ComposerView.vue`:
   - Replace the static title header with a new header layout that includes a PrimeVue `Menubar` or `Menu` anchored top-left.
   - Menu items: New Project, Save Project, Close Project.
   - Wire actions to `projectStore` + existing `handleSaveProject` method.
   - For “Close Project”, clear `activeProject` and reset dependent stores (reuse `resetProjectData`).
   - Keep a minimal title/branding element if needed, but the menu owns top-left.
2. Remove `ProjectSection` from the center workflow grid and retire the entire `workflow-grid` in the center panel.
3. Introduce a new center component (e.g., `SceneCanvas.vue`) and mount it inside `app-panel--center`.

## Phase 2: Scene Canvas (Surface Workspace)
**Objective:** Render a black 1920x1080 “scene” rectangle at 25% zoom with pan/zoom and selectable shapes.

1. Create `clients/composer/src/components/organisms/SceneCanvas.vue`:
   - Render a container that shows a 480x270 view of the 1920x1080 scene.
   - Use CSS transforms for zoom (default 0.25) and pan (translate) applied to a `scene-stage` element.
   - Render surfaces as positioned elements within the stage using `surface.vertices` (likely a quad). Start with rectangles/quads and use an SVG overlay for future polygon support.
2. Pan/zoom UX:
   - Mouse drag for pan on empty canvas; mouse wheel (or trackpad) to zoom in/out with bounds (e.g., 0.1–2.0).
   - Keep the math centered around cursor if feasible; otherwise center around canvas.
   - Store pan/zoom in local component state or a `useSceneViewport` composable for reuse.
3. Selection:
   - Clicking a surface sets it as active (e.g., `activeSurfaceId`).
   - Emit `selectSurface(surfaceId)` to parent or update a shared store (new `useSurfaceSelectionStore` or in `sceneStore`).
4. SpeedDial:
   - Add PrimeVue `SpeedDial` in the top-right of the canvas container.
   - Actions: “Add Rectangle”, “Add Quad”, (optional) “Add Triangle” or “Add Mesh”.
   - Each action appends a new `Surface` to `sceneStore.activeScene.surfaces` with default vertices and `feedId` resolved from the active/first feed (disabled if none).
5. Persistence:
   - On any surface create/update, call `sceneStore.updateScene` to persist to `/api/projects/{projectId}/scenes/{sceneId}`.

## Phase 3: Properties Panel (Right)
**Objective:** Show the selected surface’s settings (feed assignment + core properties).

1. Create `clients/composer/src/components/organisms/SurfacePropertiesPanel.vue` (or reuse `SurfaceSection` with new props):
   - If no selection: show hint text.
   - If selection: show form fields for feed assignment, opacity, brightness, blend mode, z-order.
   - Feed assignment uses existing `feedStore.feeds` to populate a Select.
2. Update `app-panel--right` to render this component and bind to selection state.
3. On field change, update the selected surface in `sceneStore.activeScene` and persist via `sceneStore.updateScene` (debounce to avoid too many requests). (Completed.)

## Phase 4: Left Browser Expansion
**Objective:** Replace placeholder tabs with actual Feed/Asset/Renderer functionality.

1. Tabs in `ComposerView.vue`:
   - Replace the current “Scenes”/“Renders” placeholder tabs with: “Feeds”, “Assets”, “Renderers”. (Completed.)
2. Feed Browser:
   - New component `clients/composer/src/components/organisms/FeedBrowser.vue`. (Completed.)
   - Use `feedStore` for CRUD. Provide list, create, delete, and selection (if needed). (Completed.)
   - On create, allow linking to an asset (reuse selected asset or include asset dropdown). (Completed: asset dropdown.)
3. Asset Browser:
   - Extend existing `AssetBrowser.vue` to add:
     - Upload (Create) using PrimeVue `FileUpload` to `/api/assets` with 2GB limit.
     - Delete action per asset (Delete) with confirmation dialog.
   - Keep read/list existing functionality. (Completed.)
4. Renderer List:
   - New component `RendererBrowser.vue` that calls `rendererStore.ping()` on mount and on refresh. (Completed.)
   - Render `rendererStore.lastStatus.renderers` with an empty-state if none. (Completed.)

## Phase 5: Server API Extensions
**Objective:** Support asset upload/delete and (if needed) a dedicated renderer list API.

1. Add API endpoints in `server/src/http/HttpServer.cpp`:
   - `POST /assets` for asset upload (multipart/form-data or raw stream).
   - Enforce a 2GB size limit (reject with 413). Store files under `data/assets/`.
   - `DELETE /assets/{name}` to remove an asset by filename.
2. Ensure path sanitization to prevent directory traversal and overwrite warnings (optional unique naming or conflict checks).
3. If preferred, add a renderer list endpoint:
   - `GET /renderer` returning `{ renderers: [...] }` (or reuse `/renderer/ping`).
4. Update tests:
   - Add `server/tests/HttpAssets_test.cpp` cases for upload + delete.
   - Extend any renderer tests if a new endpoint is added.
5. Update `AGENTS.md` if API protocol or payloads change (Rule 0).

Completed items in this phase: 1, 2, 4, 5. No new renderer endpoint added.

## Phase 6: Stores, Types, and Composables
**Objective:** Add missing state and support for new workflows.

1. Add selection state for surfaces:
   - New Pinia store `useSurfaceSelectionStore` or extend `sceneStore` with `activeSurfaceId`. (Completed: extended `sceneStore`.)
2. Add asset upload/delete actions in `clients/composer/src/stores/assetStore.ts`:
   - `uploadAsset(file: File)` → POST `/api/assets`.
   - `deleteAsset(assetId: string)` → DELETE `/api/assets/{assetId}`.
3. Add helpers for surface creation/defaults in `clients/composer/src/composables/useSurfaceFactory.ts`.
4. Update `clients/composer/src/types` if needed (e.g., add surface type info).

## Phase 7: Testing
**Objective:** Satisfy the Composer testing policy for logic changes.

1. Unit tests (Vitest):
   - `useSurfaceFactory.spec.ts` for default shapes + vertex generation.
   - `assetStore.spec.ts` for upload/delete actions (mock fetch).
   - `sceneStore.spec.ts` updates if selection is added.
2. Component tests (Vue Test Utils):
   - `SceneCanvas.spec.ts` for selection/pan/zoom logic (mock events).
   - `SurfacePropertiesPanel.spec.ts` for field binding + update behavior.
3. E2E (Playwright) for core workflow updates:
   - Create project → add shape → assign feed → save project → verify renderer list displays.

## Phase 8: UX Polish & Visuals
1. Scene canvas visuals:
   - Black surface (1920x1080) with subtle border and grid-less background.
   - Show shape outlines in a contrasting color; highlight active selection.
2. Keep the 25% zoom as default but show current zoom in the UI (small label).
3. Make sure pan/zoom works on both desktop and trackpads.

## Files Likely to Change/Add
- `clients/composer/src/views/ComposerView.vue`
- `clients/composer/src/components/organisms/SceneCanvas.vue` (new)
- `clients/composer/src/components/organisms/SurfacePropertiesPanel.vue` (new)
- `clients/composer/src/components/organisms/FeedBrowser.vue` (new)
- `clients/composer/src/components/organisms/RendererBrowser.vue` (new)
- `clients/composer/src/components/organisms/AssetBrowser.vue`
- `clients/composer/src/stores/assetStore.ts`
- `clients/composer/src/stores/sceneStore.ts` (if adding selection)
- `clients/composer/src/composables/useSurfaceFactory.ts` (new)
- `server/src/http/HttpServer.cpp`
- `server/tests/HttpAssets_test.cpp`
- `AGENTS.md` (if protocol updates)

## Open Questions / Decisions
- Confirm shape types to support now (rectangle/quad only vs. future mesh/triangle).
- Decide whether selection state lives in `sceneStore` or a new store.
- Decide on upload protocol (multipart vs. raw file streaming). Multipart likely best for PrimeVue `FileUpload`.
- Confirm whether renderer list can reuse `/renderer/ping` or needs a new endpoint.
