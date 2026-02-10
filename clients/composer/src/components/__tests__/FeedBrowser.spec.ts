import { describe, expect, it, vi } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import FeedBrowser from "../organisms/FeedBrowser.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useFeedStore } from "../../stores/feedStore";
import { useAssetStore } from "../../stores/assetStore";

const stubInputText = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
};

const stubDropdown = {
  props: ["modelValue", "options", "optionLabel", "optionValue"],
  template: `
    <select v-bind="$attrs" :value="modelValue" @change="$emit('update:modelValue', $event.target.value)">
      <option value="">Select</option>
      <option
        v-for="option in options"
        :key="option[optionValue]"
        :value="option[optionValue]"
      >
        {{ option[optionLabel] }}
      </option>
    </select>
  `,
};

const stubButton = {
  props: ["label", "loading"],
  template: "<button v-bind=\"$attrs\" @click=\"$emit('click')\">{{ label }}<slot /></button>",
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

describe("FeedBrowser", () => {
  it("creates a feed with selected asset", async () => {
    const pinia = createPinia();
    setActivePinia(pinia);

    const projectStore = useProjectStore();
    projectStore.activeProject = {
      id: "project-1",
      name: "Project",
      description: "",
      createdAt: "2026-02-02T10:00:00Z",
      updatedAt: "2026-02-02T10:00:00Z",
      assetIds: [],
      sceneIds: [],
      feedIds: [],
      cueOrder: [],
      settings: {
        controllers: {},
        midiChannels: [],
        globalConfig: {},
      },
    };

    const feedStore = useFeedStore();
    const createSpy = vi.spyOn(feedStore, "createFeed").mockResolvedValue({
      projectId: "project-1",
      id: "feed-1",
      name: "Feed",
      assetId: "asset-1",
      settings: {},
    });

    const assetStore = useAssetStore();
    assetStore.assets = [
      { id: "asset-1", name: "clip.mp4", path: "/data/assets/clip.mp4", type: "VideoFile", variants: [] },
    ];

    const wrapper = mount(FeedBrowser, {
      global: {
        plugins: [pinia],
        stubs: {
          InputText: stubInputText,
          Dropdown: stubDropdown,
          Button: stubButton,
          DataTable: stubDataTable,
          Column: stubColumn,
          Message: stubMessage,
        },
      },
    });

    await wrapper.get('[data-testid="feed-name"]').setValue("Main Feed");
    await wrapper.get('[data-testid="feed-asset"]').setValue("asset-1");
    await wrapper.get('[data-testid="feed-create"]').trigger("click");

    expect(createSpy).toHaveBeenCalledTimes(1);
    const payload = createSpy.mock.calls[0][0];
    expect(payload.projectId).toBe("project-1");
    expect(payload.name).toBe("Main Feed");
    expect(payload.assetId).toBe("asset-1");
    expect(payload.id).toMatch(/^feed-/);
  });
});
