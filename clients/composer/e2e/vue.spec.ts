import { test, expect } from "@playwright/test";

test("creates a new project from the project panel", async ({ page }) => {
  const requestPromise = page.waitForRequest((request) => {
    return request.url().includes("/api/projects") && request.method() === "POST";
  });

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
    const body = readJsonBody(route);

    if (!url.pathname.startsWith("/api/")) {
      await route.fallback();
      return;
    }

    if (url.pathname === "/api/projects" && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify([]),
      });
      return;
    }

    if (url.pathname === "/api/assets" && method === "GET") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify([]),
      });
      return;
    }

    if (url.pathname === "/api/renderer/ping" && method === "POST") {
      await route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify({ renderers: [] }),
      });
      return;
    }

    if (url.pathname === "/api/projects" && method === "POST" && body) {
      await route.fulfill({
        status: 201,
        contentType: "application/json",
        body: JSON.stringify(body),
      });
      return;
    }

    await route.fulfill({
      status: 404,
      contentType: "application/json",
      body: JSON.stringify({ error: "Unhandled test route" }),
    });
  });

  await page.goto("/");
  await page.getByRole("button", { name: "New Project" }).click();

  const dialog = page.getByRole("dialog", { name: "New Project" });
  await expect(dialog).toBeVisible();
  await dialog.locator("#project-name").fill("Stage Mapping");
  await dialog.locator("#project-description").fill("Layout test project");
  await dialog.getByRole("button", { name: "Create" }).click();

  const request = await requestPromise;
  const payload = request.postDataJSON();

  expect(payload.name).toBe("Stage Mapping");
  expect(payload.description).toBe("Layout test project");
  expect(payload.cueOrder).toEqual([]);
});
