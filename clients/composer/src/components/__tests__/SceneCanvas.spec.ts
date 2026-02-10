import { describe, expect, it, vi } from "vitest";
import { nextTick } from "vue";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import SceneCanvas from "../organisms/SceneCanvas.vue";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";

const createStores = () => {
  const pinia = createPinia();
  setActivePinia(pinia);
  return { pinia };
};

const stubSpeedDial = {
  template: "<div data-testid=\"speed-dial\"></div>",
};

const stubMessage = {
  template: "<div><slot /></div>",
};

describe("SceneCanvas", () => {
  it("selects a surface when clicked", async () => {
    const { pinia } = createStores();
    const sceneStore = useSceneStore();
    const feedStore = useFeedStore();

    feedStore.feeds = [
      {
        projectId: "proj-1",
        id: "feed-1",
        name: "Feed 1",
        assetId: "asset-1",
        settings: {},
      },
    ];

    sceneStore.scenes = [
      {
        projectId: "proj-1",
        id: "scene-1",
        name: "Scene 1",
        description: "",
        surfaces: [
          {
            id: "surface-1",
            name: "Surface 1",
            vertices: [
              { x: -0.5, y: -0.5 },
              { x: 0.5, y: -0.5 },
              { x: 0.5, y: 0.5 },
              { x: -0.5, y: 0.5 },
            ],
            feedId: "feed-1",
            opacity: 1,
            brightness: 1,
            blendMode: "Normal",
            zOrder: 0,
          },
        ],
      },
    ];
    sceneStore.activeScene = sceneStore.scenes[0];

    const wrapper = mount(SceneCanvas, {
      global: {
        plugins: [pinia],
        stubs: {
          SpeedDial: stubSpeedDial,
          Message: stubMessage,
        },
      },
    });

    await wrapper.get('[data-surface-id="surface-1"]').trigger("click");

    expect(sceneStore.activeSurfaceId).toBe("surface-1");
  });

  it("updates the zoom label on wheel", async () => {
    const { pinia } = createStores();
    const sceneStore = useSceneStore();
    const feedStore = useFeedStore();

    feedStore.feeds = [
      {
        projectId: "proj-1",
        id: "feed-1",
        name: "Feed 1",
        assetId: "asset-1",
        settings: {},
      },
    ];

    sceneStore.activeScene = {
      projectId: "proj-1",
      id: "scene-1",
      name: "Scene 1",
      description: "",
      surfaces: [],
    };

    const wrapper = mount(SceneCanvas, {
      global: {
        plugins: [pinia],
        stubs: {
          SpeedDial: stubSpeedDial,
          Message: stubMessage,
        },
      },
    });

    const viewport = wrapper.get('[data-testid="scene-viewport"]');
    Object.defineProperty(viewport.element, "getBoundingClientRect", {
      value: () =>
        ({
          left: 0,
          top: 0,
          right: 480,
          bottom: 270,
          width: 480,
          height: 270,
        }) as DOMRect,
    });

    const wheelEvent = new Event("wheel", { bubbles: true, cancelable: true }) as WheelEvent;
    Object.defineProperties(wheelEvent, {
      deltaY: { value: -100 },
      clientX: { value: 120 },
      clientY: { value: 80 },
    });
    viewport.element.dispatchEvent(wheelEvent);
    await nextTick();

    expect(wrapper.get('[data-testid="zoom-label"]').text()).not.toContain("25%");
  });

  it("drags a surface vertex", async () => {
    const { pinia } = createStores();
    const sceneStore = useSceneStore();
    const feedStore = useFeedStore();

    feedStore.feeds = [
      {
        projectId: "proj-1",
        id: "feed-1",
        name: "Feed 1",
        assetId: "asset-1",
        settings: {},
      },
    ];

    sceneStore.activeScene = {
      projectId: "proj-1",
      id: "scene-1",
      name: "Scene 1",
      description: "",
      surfaces: [
        {
          id: "surface-1",
          name: "Surface 1",
          vertices: [
            { x: -0.5, y: -0.5 },
            { x: 0.5, y: -0.5 },
            { x: 0.5, y: 0.5 },
            { x: -0.5, y: 0.5 },
          ],
          feedId: "feed-1",
          opacity: 1,
          brightness: 1,
          blendMode: "Normal",
          zOrder: 0,
        },
      ],
    };
    sceneStore.activeSurfaceId = "surface-1";
    sceneStore.updateScene = vi.fn().mockResolvedValue(sceneStore.activeScene);

    const wrapper = mount(SceneCanvas, {
      global: {
        plugins: [pinia],
        stubs: {
          SpeedDial: stubSpeedDial,
          Message: stubMessage,
        },
      },
    });

    const viewport = wrapper.get('[data-testid="scene-viewport"]');
    Object.defineProperty(viewport.element, "getBoundingClientRect", {
      value: () =>
        ({
          left: 0,
          top: 0,
          right: 480,
          bottom: 270,
          width: 480,
          height: 270,
        }) as DOMRect,
    });

    const handle = wrapper.find(".scene-canvas__handle");
    const original = sceneStore.activeScene.surfaces[0].vertices[0];

    const pointerDown = new Event("pointerdown", { bubbles: true, cancelable: true }) as PointerEvent;
    Object.defineProperties(pointerDown, {
      clientX: { value: 120 },
      clientY: { value: 80 },
      button: { value: 0 },
      pointerId: { value: 1 },
    });
    handle.element.dispatchEvent(pointerDown);

    const pointerMove = new Event("pointermove", { bubbles: true, cancelable: true }) as PointerEvent;
    Object.defineProperties(pointerMove, {
      clientX: { value: 140 },
      clientY: { value: 100 },
      button: { value: 0 },
      pointerId: { value: 1 },
    });
    window.dispatchEvent(pointerMove);

    const pointerUp = new Event("pointerup", { bubbles: true, cancelable: true }) as PointerEvent;
    Object.defineProperties(pointerUp, { pointerId: { value: 1 } });
    window.dispatchEvent(pointerUp);
    await nextTick();

    const updated = sceneStore.activeScene.surfaces[0].vertices[0];
    expect(updated.x).not.toBe(original.x);
    expect(updated.y).not.toBe(original.y);
  });
});
