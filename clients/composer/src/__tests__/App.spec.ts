import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { flushPromises, mount } from "@vue/test-utils";
import PrimeVue from "primevue/config";
import Dialog from "primevue/dialog";
import Menu from "primevue/menu";
import App from "../App.vue";

describe("App", () => {
  const fetchMock = vi.fn();
  const cryptoMock = {
    randomUUID: () => "test-uuid",
  };

  beforeEach(() => {
    vi.stubGlobal("fetch", fetchMock);
    vi.stubGlobal("crypto", cryptoMock);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    fetchMock.mockReset();
  });

  it("opens the new project dialog from the menu", async () => {
    const wrapper = mount(App, {
      global: {
        plugins: [PrimeVue],
        stubs: {
          teleport: true,
        },
      },
    });

    const menu = wrapper.findComponent(Menu);
    const model = menu.props("model") as { command?: () => void }[];
    model[0].command?.();
    await wrapper.vm.$nextTick();

    const dialog = wrapper.findComponent(Dialog);
    expect(dialog.props("visible")).toBe(true);
  });

  it("creates a project via the API", async () => {
    fetchMock.mockResolvedValue({
      ok: true,
      text: vi.fn().mockResolvedValue(""),
    });

    const wrapper = mount(App, {
      global: {
        plugins: [PrimeVue],
        stubs: {
          teleport: true,
        },
      },
    });

    const menu = wrapper.findComponent(Menu);
    const model = menu.props("model") as { command?: () => void }[];
    model[0].command?.();
    await wrapper.vm.$nextTick();

    await wrapper.find("#project-name").setValue("Test Project");
    await wrapper.find("#project-description").setValue("Project details");
    await wrapper.find("button.p-button:not(.p-button-text)").trigger("click");

    await flushPromises();

    expect(fetchMock).toHaveBeenCalledWith(
      "/api/projects",
      expect.objectContaining({
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
      }),
    );

    const body = JSON.parse(fetchMock.mock.calls[0][1].body as string);
    expect(body).toMatchObject({
      id: "project-test-uuid",
      name: "Test Project",
      description: "Project details",
      cueOrder: [],
      settings: {
        controllers: {},
        midiChannels: [],
        globalConfig: {},
      },
    });

    const dialog = wrapper.findComponent(Dialog);
    expect(dialog.props("visible")).toBe(false);
  });

  it("shows validation errors and disables create for invalid values", async () => {
    const wrapper = mount(App, {
      global: {
        plugins: [PrimeVue],
        stubs: {
          teleport: true,
        },
      },
    });

    const menu = wrapper.findComponent(Menu);
    const model = menu.props("model") as { command?: () => void }[];
    model[0].command?.();
    await wrapper.vm.$nextTick();

    const createButton = wrapper
      .findAll("button")
      .find((button) => button.text() === "Create");
    expect(createButton?.attributes("disabled")).toBeDefined();

    await wrapper.find("#project-description").setValue("a".repeat(281));
    await wrapper.vm.$nextTick();

    expect(wrapper.text()).toContain(
      "Project description must be 280 characters or less.",
    );
  });
});
