import { describe, expect, it, vi, beforeEach } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import SurfacePropertiesPanel from "../organisms/SurfacePropertiesPanel.vue";
import { useSceneStore } from "../../stores/sceneStore";
import { useFeedStore } from "../../stores/feedStore";
import { useRendererStore } from "../../stores/rendererStore";

const stubInputText = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
};

const stubInputNumber = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', Number($event.target.value))\" />",
};

const stubDropdown = {
  props: ["modelValue", "options"],
  template: `
    <select v-bind="$attrs" :value="modelValue" @change="$emit('update:modelValue', $event.target.value)">
      <option v-for="option in options" :key="option.value ?? option" :value="option.value ?? option">
        {{ option.label ?? option }}
      </option>
    </select>
  `,
};

const stubButton = {
  props: ["label", "disabled"],
  template: "<button v-bind=\"$attrs\" :disabled=\"disabled\" @click=\"$emit('click')\">{{ label }}</button>",
};

const stubMessage = {
  template: "<div><slot /></div>",
};

describe("SurfacePropertiesPanel", () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it("updates scene when properties change", async () => {
    vi.useFakeTimers();
    const sceneStore = useSceneStore();
    const feedStore = useFeedStore();

    feedStore.feeds = [
      {
        projectId: "project-1",
        id: "feed-1",
        name: "Feed 1",
        assetId: "asset-1",
        settings: {},
      },
    ];

    sceneStore.activeScene = {
      projectId: "project-1",
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

    const updateSpy = vi.spyOn(sceneStore, "updateScene").mockResolvedValue(sceneStore.activeScene);

    const wrapper = mount(SurfacePropertiesPanel, {
      global: {
        plugins: [sceneStore.$pinia],
        stubs: {
          InputText: stubInputText,
          InputNumber: stubInputNumber,
          Dropdown: stubDropdown,
          Button: stubButton,
          Message: stubMessage,
        },
      },
    });

    const inputs = wrapper.findAll("input");
    await inputs[1].setValue("0.5");
    vi.advanceTimersByTime(400);

    expect(updateSpy).toHaveBeenCalledTimes(1);
    vi.useRealTimers();
  });

  it("loads scene on preview", async () => {
    const sceneStore = useSceneStore();
    const feedStore = useFeedStore();
    const rendererStore = useRendererStore();

    feedStore.feeds = [
      {
        projectId: "project-1",
        id: "feed-1",
        name: "Feed 1",
        assetId: "asset-1",
        settings: {},
      },
    ];

    sceneStore.activeScene = {
      projectId: "project-1",
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

    const loadSpy = vi.spyOn(rendererStore, "loadScene").mockResolvedValue();

    const wrapper = mount(SurfacePropertiesPanel, {
      global: {
        plugins: [sceneStore.$pinia],
        stubs: {
          InputText: stubInputText,
          InputNumber: stubInputNumber,
          Dropdown: stubDropdown,
          Button: stubButton,
          Message: stubMessage,
        },
      },
    });

    const previewButton = wrapper.findAll("button").find((btn) => btn.text() === "Send to Renderer");
    await previewButton?.trigger("click");

    expect(loadSpy).toHaveBeenCalledWith("project-1", "scene-1");
  });
});
