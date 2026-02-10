import { describe, expect, it, beforeEach, vi } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import CueSection from "../organisms/CueSection.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useSceneStore } from "../../stores/sceneStore";
import { useCueStore } from "../../stores/cueStore";

const stubInputText = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
};

const stubDropdown = {
  props: ["modelValue", "options", "optionLabel", "optionValue"],
  template: `
    <select v-bind="$attrs" :value="modelValue" @change="$emit('update:modelValue', $event.target.value)">
      <option v-for="option in options" :key="option[optionValue]" :value="option[optionValue]">
        {{ option[optionLabel] }}
      </option>
    </select>
  `,
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

describe("CueSection", () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it("removes cue from project before deleting", async () => {
    const projectStore = useProjectStore();
    const sceneStore = useSceneStore();
    const cueStore = useCueStore();

    projectStore.activeProject = {
      id: "project-1",
      name: "Project",
      description: "",
      createdAt: "2026-02-02T10:00:00Z",
      updatedAt: "2026-02-02T10:00:00Z",
      assetIds: [],
      sceneIds: [],
      feedIds: [],
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
    cueStore.activeCue = cueStore.cues[0];

    const updateSpy = vi.spyOn(projectStore, "updateProject").mockResolvedValue(projectStore.activeProject);
    const deleteSpy = vi.spyOn(cueStore, "deleteCue").mockResolvedValue();
    vi.spyOn(window, "confirm").mockReturnValue(true);

    const wrapper = mount(CueSection, {
      global: {
        plugins: [projectStore.$pinia],
        stubs: {
          InputText: stubInputText,
          Dropdown: stubDropdown,
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

    expect(updateSpy).toHaveBeenCalledTimes(1);
    const payload = updateSpy.mock.calls[0][0];
    expect(payload.cueOrder).toEqual([]);
    expect(deleteSpy).toHaveBeenCalledWith("project-1", "cue-1");
  });

  it("selects the next cue after delete", async () => {
    const projectStore = useProjectStore();
    const cueStore = useCueStore();

    projectStore.activeProject = {
      id: "project-1",
      name: "Project",
      description: "",
      createdAt: "2026-02-02T10:00:00Z",
      updatedAt: "2026-02-02T10:00:00Z",
      assetIds: [],
      sceneIds: [],
      feedIds: [],
      cueOrder: ["cue-1", "cue-2"],
      settings: { controllers: {}, midiChannels: [], globalConfig: {} },
    };

    cueStore.cues = [
      {
        projectId: "project-1",
        id: "cue-1",
        name: "Cue 1",
        sceneId: "scene-1",
        surfaceOpacities: [],
        surfaceBrightnesses: [],
      },
      {
        projectId: "project-1",
        id: "cue-2",
        name: "Cue 2",
        sceneId: "scene-1",
        surfaceOpacities: [],
        surfaceBrightnesses: [],
      },
    ];
    cueStore.activeCue = cueStore.cues[0];

    vi.spyOn(window, "confirm").mockReturnValue(true);
    vi.spyOn(projectStore, "updateProject").mockResolvedValue(projectStore.activeProject);
    vi.spyOn(cueStore, "deleteCue").mockImplementation(async () => {
      cueStore.cues = cueStore.cues.filter((cue) => cue.id !== "cue-1");
      cueStore.activeCue = null;
    });

    const wrapper = mount(CueSection, {
      global: {
        plugins: [projectStore.$pinia],
        stubs: {
          InputText: stubInputText,
          Dropdown: stubDropdown,
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

    expect(cueStore.activeCue?.id).toBe("cue-2");
  });
});
