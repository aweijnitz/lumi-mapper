import { describe, expect, it, vi } from "vitest";
import { mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import RendererBrowser from "../organisms/RendererBrowser.vue";
import { useRendererStore } from "../../stores/rendererStore";

const stubButton = {
  props: ["label", "loading"],
  template: "<button v-bind=\"$attrs\" @click=\"$emit('click')\">{{ label }}<slot /></button>",
};

const stubMessage = {
  template: "<div><slot /></div>",
};

describe("RendererBrowser", () => {
  it("pings on mount", () => {
    const pinia = createPinia();
    setActivePinia(pinia);

    const store = useRendererStore();
    const pingSpy = vi.spyOn(store, "ping").mockResolvedValue();

    mount(RendererBrowser, {
      global: {
        plugins: [pinia],
        stubs: {
          Button: stubButton,
          Message: stubMessage,
        },
      },
    });

    expect(pingSpy).toHaveBeenCalledTimes(1);
  });
});
