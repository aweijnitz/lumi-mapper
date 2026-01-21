# lumi-composer

Projection Composer UI (Vue 3 + PrimeVue) for managing projects, scenes (including surfaces), feeds, cues, assets, and playback.

Key runtime features:
- Renderer status (ping) and live preview controls.
- Calibration grid toggle and crosshair overlay for surface alignment.
- Scene filter controls (color tint / monochrome) and palette selection.

## Recommended IDE Setup

[VS Code](https://code.visualstudio.com/) + [Vue (Official)](https://marketplace.visualstudio.com/items?itemName=Vue.volar) (and disable Vetur).

## Recommended Browser Setup

- Chromium-based browsers (Chrome, Edge, Brave, etc.):
  - [Vue.js devtools](https://chromewebstore.google.com/detail/vuejs-devtools/nhdogjmejiglipccpnnnanhbledajbpd)
  - [Turn on Custom Object Formatter in Chrome DevTools](http://bit.ly/object-formatters)
- Firefox:
  - [Vue.js devtools](https://addons.mozilla.org/en-US/firefox/addon/vue-js-devtools/)
  - [Turn on Custom Object Formatter in Firefox DevTools](https://fxdx.dev/firefox-devtools-custom-object-formatters/)

## Type Support for `.vue` Imports in TS

TypeScript cannot handle type information for `.vue` imports by default, so we replace the `tsc` CLI with `vue-tsc` for type checking. In editors, we need [Volar](https://marketplace.visualstudio.com/items?itemName=Vue.volar) to make the TypeScript language service aware of `.vue` types.

## Customize configuration

See [Vite Configuration Reference](https://vite.dev/config/).

## Project Setup

```sh
npm install
```

### Compile and Hot-Reload for Development

```sh
npm run dev
```

The dev server proxies API requests (e.g. `/api/projects`, `/api/projects/{projectId}/scenes`) to the C++ server running on `http://127.0.0.1:8080`.

Optional environment overrides:
- `VITE_API_TARGET` (Vite proxy target, default: `http://127.0.0.1:8080`)
- `VITE_API_BASE` (fetch base URL used by tests and non-proxy requests)

### Component Structure (Atomic Design)

- `src/components/atoms`: primitives like headers, labels, small badges.
- `src/components/molecules`: composed inputs/controls.
- `src/components/organisms`: workflow sections (Projects, Scenes, Feeds, Surfaces, Cues, Playback, Asset Browser).
- `src/components/templates`: layout scaffolding.
- `src/views/ComposerView.vue`: main workflow layout.

State lives in Pinia stores under `src/stores`, and HTTP lives in `src/composables/useApiClient.ts`.

### Type-Check, Compile and Minify for Production

```sh
npm run build
```

### Run Unit Tests with [Vitest](https://vitest.dev/)

```sh
npm run test:unit
```

### Run End-to-End Tests with [Playwright](https://playwright.dev)

```sh
# Install browsers for the first run
npx playwright install

# When testing on CI, must build the project first
npm run build

# Runs the end-to-end tests
npm run test:e2e
# Runs the tests only on Chromium
npm run test:e2e -- --project=chromium
# Runs the tests of a specific file
npm run test:e2e -- tests/example.spec.ts
# Runs the tests in debug mode
npm run test:e2e -- --debug
```

### Lint with [ESLint](https://eslint.org/)

```sh
npm run lint
```
