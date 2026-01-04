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
    const createdProject = {
      id: "project-test-uuid",
      name: "Test Project",
      description: "Project details",
      cueOrder: [],
      settings: {
        controllers: {},
        midiChannels: [],
        globalConfig: {},
      },
    };
    fetchMock.mockResolvedValue({
      ok: true,
      json: vi.fn().mockResolvedValue(createdProject),
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
    const createButton = wrapper
      .findAll("button")
      .find((button) => button.text() === "Create");
    await createButton?.trigger("click");

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

  it("creates feed, scene, and cue from the quickstart workflow", async () => {
    const createdProject = {
      id: "project-test-uuid",
      name: "Test Project",
      description: "Project details",
      cueOrder: [],
      settings: {
        controllers: {},
        midiChannels: [],
        globalConfig: {},
      },
    };

    const createdFeed = { id: "feed-test-uuid", name: "Clip A" };
    const createdScene = { id: "scene-test-uuid", name: "Main Scene" };
    const createdCue = { id: "cue-test-uuid", name: "Cue 1" };
    const updatedProject = {
      ...createdProject,
      cueOrder: [createdCue.id],
    };

    fetchMock.mockImplementation((url: string, options: RequestInit) => {
      if (url === "/api/projects" && options.method === "POST") {
        return Promise.resolve({
          ok: true,
          json: vi.fn().mockResolvedValue(createdProject),
          text: vi.fn().mockResolvedValue(""),
        });
      }
      if (url.endsWith("/feeds")) {
        return Promise.resolve({
          ok: true,
          json: vi.fn().mockResolvedValue(createdFeed),
          text: vi.fn().mockResolvedValue(""),
        });
      }
      if (url.endsWith("/scenes")) {
        return Promise.resolve({
          ok: true,
          json: vi.fn().mockResolvedValue(createdScene),
          text: vi.fn().mockResolvedValue(""),
        });
      }
      if (url.endsWith("/cues")) {
        return Promise.resolve({
          ok: true,
          json: vi.fn().mockResolvedValue(createdCue),
          text: vi.fn().mockResolvedValue(""),
        });
      }
      if (url === `/api/projects/${createdProject.id}`) {
        return Promise.resolve({
          ok: true,
          json: vi.fn().mockResolvedValue(updatedProject),
          text: vi.fn().mockResolvedValue(""),
        });
      }
      return Promise.resolve({
        ok: false,
        text: vi.fn().mockResolvedValue("Unhandled request"),
      });
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
    const createButton = wrapper
      .findAll("button")
      .find((button) => button.text() === "Create");
    await createButton?.trigger("click");
    await flushPromises();

    const quickCreate = wrapper.find('[data-testid="quick-create"]');
    await quickCreate.trigger("click");
    await flushPromises();

    const feedCall = fetchMock.mock.calls.find((call) =>
      (call[0] as string).endsWith("/feeds"),
    );
    const sceneCall = fetchMock.mock.calls.find((call) =>
      (call[0] as string).endsWith("/scenes"),
    );
    const cueCall = fetchMock.mock.calls.find((call) =>
      (call[0] as string).endsWith("/cues"),
    );
    const projectCall = fetchMock.mock.calls.find(
      (call) => call[0] === `/api/projects/${createdProject.id}`,
    );

    expect(feedCall).toBeDefined();
    expect(sceneCall).toBeDefined();
    expect(cueCall).toBeDefined();
    expect(projectCall).toBeDefined();

    const feedBody = JSON.parse(feedCall?.[1].body as string);
    expect(feedBody.configJson).toMatchObject({
      filePath:
        "/Users/aweijnitz/VSCODE_PROJECTS/lumi-mapper/data/assets/clipA.mp4",
    });

    const sceneBody = JSON.parse(sceneCall?.[1].body as string);
    expect(sceneBody.surfaces).toHaveLength(1);
    expect(sceneBody.surfaces[0].vertices).toEqual([
      { x: -0.5, y: -0.4 },
      { x: 0.5, y: -0.4 },
      { x: 0.5, y: 0.4 },
      { x: -0.5, y: 0.4 },
    ]);
  });
});
