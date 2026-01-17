import { test, expect } from "@playwright/test";

test("workflow creates project, feed, scene, surface, cue, saves, and plays", async ({ page }) => {
  const state = {
    projectId: "",
    feeds: [] as Array<{ id: string; name: string; type: string; configJson: { filePath: string } }>,
    scenes: [] as Array<{ id: string; name: string; description: string; surfaces: unknown[] }>,
    cues: [] as Array<{ id: string; name: string; sceneId: string }>,
  };

  const readJsonBody = (route: Parameters<Parameters<typeof page.route>[1]>[0]) => {
    try {
      return route.request().postDataJSON();
    } catch {
      return null;
    }
  };

  await page.route("**/*", async (route) => {
    const url = new URL(route.request().url());
    const method = route.request().method();
    const pathname = url.pathname;
    const body = readJsonBody(route);

    if (!pathname.startsWith("/api/")) {
      await route.fallback();
      return;
    }

    if (pathname === "/api/projects" && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify([]),
      });
      return;
    }

    if (pathname === "/api/assets" && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify([
          { id: "asset-1", name: "Clip A", path: "/data/assets/clipA.mp4", type: "video" },
        ]),
      });
      return;
    }

    if (pathname === "/api/projects" && method === "POST" && body) {
      state.projectId = body.id;
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/scenes") && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify(state.scenes),
      });
      return;
    }

    if (pathname.endsWith("/feeds") && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify(state.feeds),
      });
      return;
    }

    if (pathname.endsWith("/cues") && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify(state.cues),
      });
      return;
    }

    if (pathname.endsWith("/feeds") && method === "POST" && body) {
      state.feeds.push(body);
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/scenes") && method === "POST" && body) {
      state.scenes.push(body);
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.includes("/scenes/") && method === "PUT" && body) {
      state.scenes = state.scenes.map((scene) => (scene.id === body.id ? body : scene));
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/cues") && method === "POST" && body) {
      state.cues.push(body);
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (state.projectId && pathname === `/api/projects/${state.projectId}` && body) {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (
      state.projectId &&
      pathname === `/api/projects/${state.projectId}/renderer/playCue` &&
      method === "POST" &&
      body
    ) {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify({ status: "sent", cueId: body.cueId }),
      });
      return;
    }

    await route.fulfill({
      status: 404,
      contentType: "application/json",
      body: JSON.stringify({ error: "Unhandled test route" }),
    });
  });

  const feedRequestPromise = page.waitForRequest((request) =>
    request.url().includes("/feeds") && request.method() === "POST",
  );
  const sceneRequestPromise = page.waitForRequest((request) =>
    request.url().includes("/scenes") && request.method() === "POST",
  );
  const initialFeedsRequest = page.waitForRequest((request) =>
    request.url().includes("/feeds") && request.method() === "GET",
  );
  const initialScenesRequest = page.waitForRequest((request) =>
    request.url().includes("/scenes") && request.method() === "GET",
  );
  const initialCuesRequest = page.waitForRequest((request) =>
    request.url().includes("/cues") && request.method() === "GET",
  );
  const cueRequestPromise = page.waitForRequest((request) =>
    request.url().includes("/cues") && request.method() === "POST",
  );
  const projectUpdatePromise = page.waitForRequest((request) => {
    if (request.method() !== "PUT") {
      return false;
    }
    const url = new URL(request.url());
    return url.pathname === `/api/projects/${state.projectId}`;
  });

  await page.goto("/");

  await page.getByRole("button", { name: "New Project" }).click();
  await page.getByLabel("Project name").fill("Stage Mapping");
  await page.getByLabel("Project description").fill("Layout test project");
  const dialog = page.getByRole("dialog", { name: "New Project" });
  await dialog.getByRole("button", { name: "Create" }).click();
  await Promise.all([initialFeedsRequest, initialScenesRequest, initialCuesRequest]);

  await page.getByPlaceholder("Search assets").fill("Clip");
  await page.getByRole("row", { name: /Clip A/ }).click();

  await page.getByPlaceholder("Feed name").fill("Main Feed");
  await page.getByRole("button", { name: "Add Feed" }).click();
  await feedRequestPromise;
  await expect(page.getByRole("row", { name: /Main Feed/ })).toBeVisible();

  await page.getByPlaceholder("Scene name").fill("Main Scene");
  await page.getByPlaceholder("Scene description").fill("Primary scene");
  await page.getByRole("button", { name: "Add Scene" }).click();
  await sceneRequestPromise;
  await expect(page.getByRole("row", { name: /Main Scene/ })).toBeVisible();
  await page.getByRole("row", { name: /Main Scene/ }).click();
  const surfaceSceneDropdown = page
    .locator(".surface-section")
    .getByRole("combobox", { name: "Select a scene" });
  await surfaceSceneDropdown.click();
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");

  await page.getByPlaceholder("Surface name").fill("Center Surface");
  const feedDropdown = page
    .locator(".surface-section")
    .getByRole("combobox", { name: "Select a feed" });
  await feedDropdown.click();
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  const sceneUpdatePromise = page.waitForRequest((request) =>
    request.url().includes("/scenes") && request.method() === "PUT",
  );
  await page.getByRole("button", { name: "Add Surface" }).click();

  await page.getByPlaceholder("Cue name").fill("Cue 1");
  const sceneDropdown = page
    .locator(".cue-section")
    .getByRole("combobox", { name: "Select scene" });
  await sceneDropdown.click();
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  await page.getByRole("button", { name: "Add Cue" }).click();

  await page.getByRole("button", { name: "Save Project" }).click();

  const feedPayload = (await feedRequestPromise).postDataJSON();
  const scenePayload = (await sceneRequestPromise).postDataJSON();
  const sceneUpdatePayload = (await sceneUpdatePromise).postDataJSON();
  const cuePayload = (await cueRequestPromise).postDataJSON();
  const projectPayload = (await projectUpdatePromise).postDataJSON();

  expect(feedPayload.configJson.filePath).toBe("/data/assets/clipA.mp4");
  expect(scenePayload.surfaces).toHaveLength(0);
  expect(sceneUpdatePayload.surfaces).toHaveLength(1);
  expect(cuePayload.sceneId).toBe(scenePayload.id);
  expect(projectPayload.cueOrder).toContain(cuePayload.id);

  const playCueRequestPromise = page.waitForRequest((request) =>
    request.url().includes("/renderer/playCue") && request.method() === "POST",
  );
  await page.getByRole("button", { name: "Play Cue" }).click();
  const playRequest = await playCueRequestPromise;
  expect(playRequest.postDataJSON().cueId).toBe(cuePayload.id);
});
