import { test, expect } from '@playwright/test'

test('creates a new project from the menu', async ({ page }) => {
  const requestPromise = page.waitForRequest((request) => {
    return request.url().includes('/api/projects') && request.method() === 'POST'
  })

  await page.route('**/api/projects', async (route) => {
    if (route.request().method() === 'POST') {
      await route.fulfill({
        status: 201,
        contentType: 'application/json',
        body: JSON.stringify({ ok: true }),
      })
      return
    }
    await route.fallback()
  })

  await page.goto('/')
  await page.getByRole('button', { name: 'Project' }).click()
  const newProjectItem = page.getByRole('menuitem', { name: 'New Project' })
  await expect(newProjectItem).toBeVisible()
  await newProjectItem.click()

  await page.getByLabel('Project name').fill('Stage Mapping')
  await page.getByLabel('Project description').fill('Layout test project')
  const dialog = page.getByRole('dialog', { name: 'New Project' })
  await dialog.getByRole('button', { name: 'Create', exact: true }).click()

  const request = await requestPromise
  const payload = request.postDataJSON()

  expect(payload.name).toBe('Stage Mapping')
  expect(payload.description).toBe('Layout test project')
  expect(payload.cueOrder).toEqual([])
})
