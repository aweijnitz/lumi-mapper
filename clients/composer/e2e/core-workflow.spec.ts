import { test, expect } from "@playwright/test";

test("quickstart creates feed, scene, cue, and plays a cue", async ({ page }) => {
  const state = {
    projectId: "",
    cueId: "",
  };

  await page.route("**/api/**", async (route) => {
    const url = new URL(route.request().url());
    const method = route.request().method();
    const pathname = url.pathname;
    const body = route.request().postDataJSON();

    if (pathname === "/api/projects" && method === "POST") {
      state.projectId = body.id;
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/feeds") && method === "POST") {
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/scenes") && method === "POST") {
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (pathname.endsWith("/cues") && method === "POST") {
      state.cueId = body.id;
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    if (state.projectId && pathname === `/api/projects/${state.projectId}`) {
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
      method === "POST"
    ) {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify({ status: "sent", cueId: body.cueId }),
      });
      return;
    }

    await route.fallback();
  });

  const feedRequestPromise = page.waitForRequest((request) => {
    return request.url().includes("/feeds") && request.method() === "POST";
  });
  const sceneRequestPromise = page.waitForRequest((request) => {
    return request.url().includes("/scenes") && request.method() === "POST";
  });
  const cueRequestPromise = page.waitForRequest((request) => {
    return request.url().includes("/cues") && request.method() === "POST";
  });
  const projectUpdatePromise = page.waitForRequest((request) => {
    return (
      request.method() === "PUT" && request.url().includes("/api/projects/")
    );
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Project" }).click();
  await page.getByRole("menuitem", { name: "New Project" }).click();
  await page.getByLabel("Project name").fill("Stage Mapping");
  await page.getByLabel("Project description").fill("Layout test project");
  const dialog = page.getByRole("dialog", { name: "New Project" });
  await dialog.getByRole("button", { name: "Create", exact: true }).click();

  await page.getByRole("button", { name: "Create Feed + Scene + Cue" }).click();

  const feedRequest = await feedRequestPromise;
  const sceneRequest = await sceneRequestPromise;
  const cueRequest = await cueRequestPromise;
  const projectRequest = await projectUpdatePromise;

  const feedPayload = feedRequest.postDataJSON();
  const scenePayload = sceneRequest.postDataJSON();
  const cuePayload = cueRequest.postDataJSON();
  const projectPayload = projectRequest.postDataJSON();

  expect(feedPayload.configJson.filePath).toBe(
    "/Users/aweijnitz/VSCODE_PROJECTS/lumi-mapper/data/assets/clipA.mp4",
  );
  expect(scenePayload.surfaces).toHaveLength(1);
  expect(cuePayload.sceneId).toBe(scenePayload.id);
  expect(projectPayload.cueOrder).toContain(cuePayload.id);

  const playCueRequestPromise = page.waitForRequest((request) => {
    return request.url().includes("/renderer/playCue") && request.method() === "POST";
  });
  await page.getByRole("button", { name: "Play Cue" }).click();
  const playRequest = await playCueRequestPromise;
  expect(playRequest.postDataJSON().cueId).toBe(cuePayload.id);
});
