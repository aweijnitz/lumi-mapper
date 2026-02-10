import { describe, expect, it, vi, beforeEach } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import AssetBrowser from "../organisms/AssetBrowser.vue";
import { useAssetStore } from "../../stores/assetStore";

const stubInputText = {
  props: ["modelValue"],
  template:
    "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
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

const stubFileUpload = {
  template: "<button data-testid=\"file-upload\" @click=\"$emit('uploader', { files: mockFiles })\">Upload</button>",
  data() {
    return { mockFiles: [new File(['dummy'], 'clip.mp4', { type: 'video/mp4' })] };
  },
};

describe("AssetBrowser", () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it("uploads files via asset store", async () => {
    const store = useAssetStore();
    const uploadSpy = vi.spyOn(store, "uploadAsset").mockResolvedValue();

    const wrapper = mount(AssetBrowser, {
      global: {
        plugins: [store.$pinia],
        stubs: {
          InputText: stubInputText,
          Button: stubButton,
          DataTable: stubDataTable,
          Column: stubColumn,
          Message: stubMessage,
          FileUpload: stubFileUpload,
        },
      },
    });

    await wrapper.get('[data-testid="asset-upload"]').trigger("click");

    expect(uploadSpy).toHaveBeenCalledTimes(1);
  });

  it("deletes selected asset after confirmation", async () => {
    const store = useAssetStore();
    store.assets = [
      { id: "clip.mp4", name: "clip.mp4", path: "/data/clip.mp4", type: "VideoFile", variants: [] },
    ];
    store.activeAsset = store.assets[0];
    const deleteSpy = vi.spyOn(store, "deleteAsset").mockResolvedValue();

    const confirmSpy = vi.spyOn(window, "confirm").mockReturnValue(true);

    const wrapper = mount(AssetBrowser, {
      global: {
        plugins: [store.$pinia],
        stubs: {
          InputText: stubInputText,
          Button: stubButton,
          DataTable: stubDataTable,
          Column: stubColumn,
          Message: stubMessage,
          FileUpload: stubFileUpload,
        },
      },
    });

    const deleteButtons = wrapper.findAll("button").filter((btn) => btn.text() === "Delete");
    await deleteButtons[0].trigger("click");

    expect(confirmSpy).toHaveBeenCalledTimes(1);
    expect(deleteSpy).toHaveBeenCalledWith("clip.mp4");
  });
});
