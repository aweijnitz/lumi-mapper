import { describe, expect, it, vi } from "vitest";
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
        type: "VideoFile",
        configJson: { filePath: "/data/assets/clip.mp4" },
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
        type: "VideoFile",
        configJson: { filePath: "/data/assets/clip.mp4" },
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

    await viewport.trigger("wheel", {
      deltaY: -100,
      clientX: 120,
      clientY: 80,
    });

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
        type: "VideoFile",
        configJson: { filePath: "/data/assets/clip.mp4" },
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

    await handle.trigger("pointerdown", {
      clientX: 120,
      clientY: 80,
      button: 0,
      pointerId: 1,
    });
    await viewport.trigger("pointermove", {
      clientX: 140,
      clientY: 100,
      button: 0,
      pointerId: 1,
    });
    await viewport.trigger("pointerup", { pointerId: 1 });

    const updated = sceneStore.activeScene.surfaces[0].vertices[0];
    expect(updated.x).not.toBe(original.x);
    expect(updated.y).not.toBe(original.y);
  });
});
