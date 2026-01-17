import { describe, expect, it, beforeEach, vi } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import SceneSection from "../organisms/SceneSection.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useSceneStore } from "../../stores/sceneStore";
import { useCueStore } from "../../stores/cueStore";

const stubInputText = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
};

const stubTextarea = {
  props: ["modelValue"],
  template:
    "<textarea v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
};

const stubButton = {
  props: ["label", "disabled"],
  template: "<button v-bind=\"$attrs\" :disabled=\"disabled\" @click=\"$emit('click')\">{{ label }}</button>",
};

const stubDataTable = {
  template: "<div><slot /></div>",
};

const stubColumn = {
  template: "<div></div>",
};

const stubMessage = {
  template: "<div><slot /></div>",
};

const stubSectionHeader = {
  template: "<div></div>",
};

describe("SceneSection", () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it("blocks delete when a cue references the scene", async () => {
    const projectStore = useProjectStore();
    const sceneStore = useSceneStore();
    const cueStore = useCueStore();

    projectStore.activeProject = {
      id: "project-1",
      name: "Project",
      description: "",
      cueOrder: ["cue-1"],
      settings: { controllers: {}, midiChannels: [], globalConfig: {} },
    };

    sceneStore.scenes = [
      {
        projectId: "project-1",
        id: "scene-1",
        name: "Scene 1",
        description: "",
        surfaces: [],
      },
    ];
    sceneStore.activeScene = sceneStore.scenes[0];

    cueStore.cues = [
      {
        projectId: "project-1",
        id: "cue-1",
        name: "Cue 1",
        sceneId: "scene-1",
        surfaceOpacities: [],
        surfaceBrightnesses: [],
      },
    ];

    const deleteSpy = vi.spyOn(sceneStore, "deleteScene").mockResolvedValue();

    const wrapper = mount(SceneSection, {
      global: {
        plugins: [projectStore.$pinia],
        stubs: {
          InputText: stubInputText,
          Textarea: stubTextarea,
          Button: stubButton,
          DataTable: stubDataTable,
          Column: stubColumn,
          Message: stubMessage,
          SectionHeader: stubSectionHeader,
        },
      },
    });

    const deleteButton = wrapper.findAll("button").find((btn) => btn.text() === "Delete");
    await deleteButton?.trigger("click");

    expect(deleteSpy).not.toHaveBeenCalled();
    expect(wrapper.text()).toContain("Cannot delete a scene referenced by a cue.");
  });

  it("selects the next scene after delete", async () => {
    const projectStore = useProjectStore();
    const sceneStore = useSceneStore();
    const cueStore = useCueStore();

    projectStore.activeProject = {
      id: "project-1",
      name: "Project",
      description: "",
      cueOrder: [],
      settings: { controllers: {}, midiChannels: [], globalConfig: {} },
    };

    sceneStore.scenes = [
      {
        projectId: "project-1",
        id: "scene-1",
        name: "Scene 1",
        description: "",
        surfaces: [],
      },
      {
        projectId: "project-1",
        id: "scene-2",
        name: "Scene 2",
        description: "",
        surfaces: [],
      },
    ];
    sceneStore.activeScene = sceneStore.scenes[0];
    cueStore.cues = [];

    vi.spyOn(window, "confirm").mockReturnValue(true);
    const deleteSpy = vi.spyOn(sceneStore, "deleteScene").mockImplementation(async () => {
      sceneStore.scenes = sceneStore.scenes.filter((scene) => scene.id !== "scene-1");
      sceneStore.activeScene = null;
    });

    const wrapper = mount(SceneSection, {
      global: {
        plugins: [projectStore.$pinia],
        stubs: {
          InputText: stubInputText,
          Textarea: stubTextarea,
          Button: stubButton,
          DataTable: stubDataTable,
          Column: stubColumn,
          Message: stubMessage,
          SectionHeader: stubSectionHeader,
        },
      },
    });

    const deleteButton = wrapper.findAll("button").find((btn) => btn.text() === "Delete");
    await deleteButton?.trigger("click");

    expect(deleteSpy).toHaveBeenCalledTimes(1);
    expect(sceneStore.activeScene?.id).toBe("scene-2");
  });
});
