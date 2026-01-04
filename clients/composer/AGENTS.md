# Composer (clients/composer)

**Purpose:** Composer is a Vue 3 + Vite application that provides the UI for building and managing projection scenes.

---

## 🔧 Tech stack & tooling

- Vue 3 (Composition API, <script setup>)
- TypeScript (strict mode recommended)
- Vite
- PrimeVue (UI components)
- Vitest + Vue Test Utils (unit tests)
- Playwright (end-to-end tests)
- ESLint + Prettier (formatting and linting)

---

## ✅ Core conventions & clean-code rules

Follow these rules to keep the codebase maintainable and predictable:

- Use the **Composition API** with `<script setup lang="ts">` for new components. Prefer smaller single-responsibility components.
- Keep templates declarative: no heavy business logic in templates — move logic into `setup()` / computed properties or composables.
- Extract reusable logic into **composables** (e.g., `src/composables/*`). Composables must be pure where possible and easy to unit-test.
- Use **Pinia** for application-wide state (if needed). Keep stores focused and small.
- Types: Use TypeScript for all components and composables; export and reuse types from `src/types` when appropriate.
- Props & emits: Explicitly type props and emits. Keep the component API minimal and consistent.
- Style: prefer `scoped` styles for component-local CSS. Prefer utility classes or theme-level CSS variables over component-global styles.
- Accessibility: follow a11y best-practices (labels, ARIA attributes, keyboard navigation) for interactive components.
- Naming: Component files use PascalCase (e.g. `SurfaceEditor.vue`), composables use `useXxx` (e.g. `useSceneStore.ts`).

> 💡 Tip: keep each function < 50–80 lines and each component < 300 lines for easier review and testing.

---

## 🧪 Testing policy (MANDATORY)

**Strict rule:** For any component or module that contains logic (business rules, DOM event handling beyond trivial, state transitions, formatting, calculations, API adapters etc.), **unit tests are mandatory**.

- Unit tests: use **Vitest** + **@vue/test-utils**.
  - Store tests: `src/stores/*.spec.ts`
  - Composable tests: `src/composables/*.spec.ts`
  - Component tests: `src/components/__tests__/*.spec.ts` (or next to the component as `Component.spec.ts`)
  - Keep tests small and deterministic (mock external dependencies, avoid network I/O).
  - Enforce a minimum coverage threshold (e.g. 80% overall; raise for critical modules).

- End-to-end tests: **Playwright** is required for all *core workflows* (examples below). Core workflows must have at least one stable E2E test that covers the happy path.
  - Store e2e tests in `tests/e2e` or Playwright's `e2e` folder.
  - Core workflow examples:
    - Create a project -> add feeds -> map a surface -> play a cue
    - Import assets -> assign to feed -> verify rendering preview
    - User permissions & save/load project

- CI requirement: Pull requests must include passing unit tests and E2E tests (or a green gate that is marked as flaky and tracked separately). Linting must pass.

---

## 🧩 Test structure & example scripts

Add these scripts to `package.json` (or ensure they exist):

- `test:unit` -> `vitest --run`
- `test:e2e` -> `playwright test`
- `test` -> `pnpm run test:unit && pnpm run test:e2e`
- `lint` -> `eslint "src/**/*.{ts,vue}" --fix`

Example test file conventions:

- `src/composables/useFoo.spec.ts` — unit tests for composables
- `src/components/SomeWidget.spec.ts` — shallow/mount tests for components
- `tests/e2e/core-workflow.spec.ts` — Playwright end-to-end tests for a critical workflow

---

## ⚖️ PR & review policy

- Every feature or bugfix affecting logic must include unit tests and, if it touches a core workflow, an E2E test.
- PRs should be small and focused. Add a brief description of functional changes and testing performed.
- Maintainers may require changes if a PR introduces untested logic.

---

## 🔍 Linting, formatting and quality

- Use ESLint with `plugin:vue/vue3-recommended` and `@typescript-eslint` rules.
- Run `eslint` and `prettier` locally before opening a PR.
- Consider adding Husky pre-commit hooks to run linting and `vitest` (fast checks) locally.

---

## 📂 Suggested folder layout

- `src/components` — presentational & small stateful components
- `src/views` — route-level views and page composition
- `src/composables` — reusable logic hooks
- `src/stores` — Pinia stores
- `src/types` — shared TypeScript types
- `tests/e2e` — Playwright tests

---

## 📌 Summary (short)

- Use Vue 3 + Composition API with TypeScript and keep components small and testable. ✅
- **Unit tests are mandatory** for any logic. ✅
- **E2E tests are required** for core workflows. ✅
- Linting, formatting, and CI checks must be green before merging. ✅

---

## 📚 Further reading

- Vue 3 style guide: https://vuejs.org/style-guide/
- Vitest + Vue Test Utils docs
- Playwright best practices
- PrimeVue component docs


# PrimeVue documentation links

## Guides

- [Introduction](https://primevue.org/introduction): Next-generation UI Component suite for Vue.
- [Configuration](https://primevue.org/configuration): Application wide configuration for PrimeVue.
- [Styled Mode](https://primevue.org/styled): Choose from a variety of pre-styled themes or develop your own.
- [Unstyled Mode](https://primevue.org/unstyled): Theming PrimeVue with alternative styling approaches.
- [Pass Through](https://primevue.org/passthrough): The Pass Through attributes is an API to access the internal DOM Structure of the components.
- [Icons](https://primevue.org/icons): PrimeIcons is the default icon library of PrimeVue with over 250 open source icons developed by PrimeTek. PrimeIcons library is optional as PrimeVue components can use any icon with templating.
- [Custom Icons](https://primevue.org/customicons): PrimeVue components can be used with any icon library using the templating features.
- [Forms](https://primevue.org/forms): The PrimeVue Forms library provides comprehensive form state management with built-in validation support.
- [Auto Import](https://primevue.org/autoimport): On-demand PrimeVue components with auto imports and tree-shaking.
- [Install PrimeVue with CND](https://primevue.org/cdn): Setting up PrimeVue in a project using CDN.
- [Install PrimeVue with Laravel](https://primevue.org/laravel): Setting up PrimeVue in a Laravel project.
- [Install PrimeVue with Nuxt](https://primevue.org/nuxt): Setting up PrimeVue in a Nuxt project.
- [Install PrimeVue with Vite](https://primevue.org/vite): Setting up PrimeVue in a Vite project.
- [Terms and Conditions](https://primevue.org/designer): Theme Designer is the ultimate tool to customize and design your own themes featuring a visual editor, figma to theme code, cloud storage, and migration assistant.
- [Tailwind CSS](https://primevue.org/tailwind): Integration between PrimeVue and Tailwind CSS both in styled and unstyled modes.
- [UI Kit](https://primevue.org/uikit): Design files for PrimeVue Components.
- [Contribution Guide](https://primevue.org/contribution): Welcome to the PrimeVue Contribution Guide and thank you for considering contributing.
- [Setup](https://primevue.org/setup): Installation guides for popular development environments.
- [LLMs.txt](https://primevue.org/llms): LLM-optimized documentation endpoints for PrimeVue components.
- [MCP Server](https://primevue.org/mcp): Model Context Protocol (MCP) server for PrimeVue component library. Provides AI assistants with comprehensive access to PrimeVue component documentation.
- [Accessibility](https://primevue.org/guides/accessibility): PrimeVue has WCAG 2.1 AA level compliance, refer to the accessibility documentation of each component for detailed information.
- [Animations](https://primevue.org/guides/animations): Various PrimeVue Components utilize native CSS animations to provide an enhanced user experience. The default animations are based on the best practices recommended by the usability experts. In case you need to customize the default animations, this documentation covers the entire set of built-in animations.
- [Dynamic Imports](https://primevue.org/guides/dynamicimports): Dynamic imports enable the loading of multiple items as needed, streamlining the import process.
- [Migration](https://primevue.org/guides/migration/v4): Migration guide to PrimeVue v4.
- [RTL Support](https://primevue.org/guides/rtl): Right-to-left direction support of PrimeVue.

## Components

- [mcp](https://primevue.org/mcp): 
- [Vue Accordion Component](https://primevue.org/accordion): Accordion groups a collection of contents in panels.
- [Vue AnimateOnScroll Directive](https://primevue.org/animateonscroll): AnimateOnScroll is used to apply animations to elements when entering or leaving the viewport during scrolling.
- [Vue AutoComplete Component](https://primevue.org/autocomplete): AutoComplete is an input component that provides real-time suggestions when being typed.
- [Vue Avatar Component](https://primevue.org/avatar): Avatar represents people using icons, labels and images.
- [Vue Badge Component](https://primevue.org/badge): Badge is a small status indicator for another element.
- [Vue BlockUI Component](https://primevue.org/blockui): BlockUI can either block other components or the whole page.
- [Vue Breadcrumb Component](https://primevue.org/breadcrumb): Breadcrumb provides contextual information about page hierarchy.
- [Vue Button Component](https://primevue.org/button): Button is an extension to standard input element with icons and theming.
- [Vue Card Component](https://primevue.org/card): Card is a flexible container component.
- [Vue Carousel Component](https://primevue.org/carousel): Carousel is a content slider featuring various customization options.
- [Vue CascadeSelect Component](https://primevue.org/cascadeselect): CascadeSelect is a form component to select a value from a nested structure of options.
- [Vue Chart Component](https://primevue.org/chart): Chart components are based on Chart.js, an open source HTML5 based charting library.
- [Vue Checkbox Component](https://primevue.org/checkbox): Checkbox is an extension to standard checkbox element with theming.
- [Vue Chip Component](https://primevue.org/chip): Chip represents entities using icons, labels and images.
- [Vue ColorPicker Component](https://primevue.org/colorpicker): ColorPicker is an input component to select a color.
- [Vue Confirmation Dialog Component](https://primevue.org/confirmdialog): ConfirmDialog uses a Dialog UI that is integrated with the Confirmation API.
- [Vue Confirmation Popup Component](https://primevue.org/confirmpopup): ConfirmPopup displays a confirmation overlay displayed relatively to its target.
- [Vue ContextMenu Component](https://primevue.org/contextmenu): ContextMenu displays an overlay menu to display actions related to an element.
- [Vue DataView Component](https://primevue.org/dataview): DataView displays data in grid or list layout with pagination and sorting features.
- [Vue DatePicker Component](https://primevue.org/datepicker): DatePicker is a form component for date inputs.
- [Vue Deferred Content Component](https://primevue.org/deferredcontent): DeferredContent postpones the loading the content that is initially not in the viewport until it becomes visible on scroll.
- [Vue Dialog Component](https://primevue.org/dialog): Dialog is a container to display content in an overlay window.
- [Vue Divider Component](https://primevue.org/divider): Divider is used to separate contents.
- [Vue Dock Component](https://primevue.org/dock): Dock is a navigation component consisting of menuitems.
- [Vue Drawer Component](https://primevue.org/drawer): Drawer is a container component displayed as an overlay.
- [Vue Dynamic Dialog Component](https://primevue.org/dynamicdialog): Dialogs can be created dynamically with any component as the content using a DialogService.
- [Vue Editor Component](https://primevue.org/editor): Editor is rich text editor component based on Quill.
- [Vue Fieldset Component](https://primevue.org/fieldset): Fieldset is a grouping component with a content toggle feature.
- [Vue File Upload Component](https://primevue.org/fileupload): FileUpload is an advanced uploader with dragdrop support, multi file uploads, auto uploading, progress tracking and validations.
- [Vue Float Label](https://primevue.org/floatlabel): FloatLabel visually integrates a label with its form element.
- [Vue Fluid Component](https://primevue.org/fluid): Fluid is a layout component to make descendant components span full width of their container.
- [Vue FocusTrap Directive](https://primevue.org/focustrap): Focus Trap keeps focus within a certain DOM element while tabbing.
- [Vue Gallery Component](https://primevue.org/galleria): Galleria is a content gallery component.
- [Vue IconField](https://primevue.org/iconfield): IconField wraps an input and an icon.
- [Vue Ifta Label](https://primevue.org/iftalabel): IftaLabel is used to create infield top aligned labels
- [Vue Image Component](https://primevue.org/image): Displays a single image with preview and tranformation options.
- [Vue ImageCompare Component](https://primevue.org/imagecompare): Compare two images side by side with a slider.
- [Vue Inplace Component](https://primevue.org/inplace): Inplace provides an easy to do editing and display at the same time where clicking the output displays the actual content.
- [Vue Input Component](https://primevue.org/inputtext): InputText is an extension to standard input element with theming.
- [Vue InputGroup Component](https://primevue.org/inputgroup): Text, icon, buttons and other content can be grouped next to an input.
- [Vue InputNumber Component](https://primevue.org/inputnumber): InputNumber is an input component to provide numerical input.
- [Vue KeyFilter Component](https://primevue.org/keyfilter): A keyfilter is a directive used to block individual keystrokes based on a pattern.
- [Vue Knob Component](https://primevue.org/knob): Knob is a form component to define number inputs with a dial.
- [Vue Listbox Component](https://primevue.org/listbox): Listbox is used to select one or more values from a list of items.
- [Vue Mask Component](https://primevue.org/inputmask): InputMask component is used to enter input in a certain format such as numeric, date, currency, email and phone.
- [Vue MegaMenu Component](https://primevue.org/megamenu): MegaMenu is a navigation component that displays submenus and content in columns.
- [Vue Menu Component](https://primevue.org/menu): Menu displays a list of items in vertical orientation.
- [Vue Message Component](https://primevue.org/message): Message component is used to display inline messages.
- [Vue MeterGroup Component](https://primevue.org/metergroup): MeterGroup displays scalar measurements within a known range.
- [Vue MultiSelect Component](https://primevue.org/multiselect): MultiSelect is used to select multiple items from a collection.
- [Vue Navbar Component](https://primevue.org/menubar): Menubar also known as Navbar, is a horizontal menu component.
- [Vue OrderList Component](https://primevue.org/orderlist): OrderList is used to sort a collection.
- [Vue Organization Chart Component](https://primevue.org/organizationchart): OrganizationChart visualizes hierarchical organization data.
- [Vue Otp Input Component](https://primevue.org/inputotp): Input Otp is used to enter one time passwords.
- [Vue Paginator Component](https://primevue.org/paginator): Paginator displays data in paged format and provides navigation between pages.
- [Vue Panel Component](https://primevue.org/panel): Panel is a grouping component providing with content toggle feature.
- [Vue PanelMenu Component](https://primevue.org/panelmenu): PanelMenu is a hybrid of accordion-tree components.
- [Vue Password Component](https://primevue.org/password): Password displays strength indicator for password fields.
- [Vue PickList Component](https://primevue.org/picklist): PickList is used to reorder items between different lists.
- [Vue Popover Component](https://primevue.org/popover): Popover is a container component that can overlay other components on page.
- [Vue ProgressBar Component](https://primevue.org/progressbar): ProgressBar is a process status indicator.
- [Vue ProgressSpinner Component](https://primevue.org/progressspinner): ProgressSpinner is a process status indicator.
- [Vue RadioButton Component](https://primevue.org/radiobutton): RadioButton is an extension to standard radio button element with theming.
- [Vue Rating Component](https://primevue.org/rating): Rating component is a star based selection input.
- [Vue Ripple Component](https://primevue.org/ripple): Ripple directive adds ripple effect to the host element.
- [Vue ScrollPanel Component](https://primevue.org/scrollpanel): ScrollPanel is a cross browser, lightweight and skinnable alternative to native browser scrollbar.
- [Vue ScrollTop Component](https://primevue.org/scrolltop): ScrollTop gets displayed after a certain scroll position and used to navigates to the top of the page quickly.
- [Vue Select Component](https://primevue.org/select): Select is used to choose an item from a collection of options.
- [Vue SelectButton Component](https://primevue.org/selectbutton): SelectButton is used to choose single or multiple items from a list using buttons.
- [Vue Skeleton Component](https://primevue.org/skeleton): Skeleton is a placeholder to display instead of the actual content.
- [Vue Slider Component](https://primevue.org/slider): Slider is a component to provide input with a drag handle.
- [Vue Speed Dial Component](https://primevue.org/speeddial): When pressed, a floating action button can display multiple primary actions that can be performed on a page.
- [Vue SplitButton Component](https://primevue.org/splitbutton): SplitButton groups a set of commands in an overlay with a default command.
- [Vue Splitter Component](https://primevue.org/splitter): Splitter is utilized to separate and resize panels.
- [Vue Stepper Component](https://primevue.org/stepper): The Stepper component displays a wizard-like workflow by guiding users through the multi-step progression.
- [Vue StyleClass Directive](https://primevue.org/styleclass): StyleClass manages css classes declaratively to during enter/leave animations or just to toggle classes on an element.
- [Vue Table Component](https://primevue.org/datatable): DataTable displays data in tabular format.
- [Vue Tabs Component](https://primevue.org/tabs): Tabs facilitates seamless switching between different views.
- [Vue Tag Component](https://primevue.org/tag): Tag component is used to categorize content.
- [Vue Terminal Component](https://primevue.org/terminal): Terminal is a text based user interface.
- [Vue Textarea Component](https://primevue.org/textarea): Textarea adds styling and autoResize functionality to standard textarea element.
- [Vue TieredMenu Component](https://primevue.org/tieredmenu): TieredMenu displays submenus in nested overlays.
- [Vue Timeline Component](https://primevue.org/timeline): Timeline visualizes a series of chained events.
- [Vue Toast Component](https://primevue.org/toast): Toast is used to display messages in an overlay.
- [Vue ToggleButton Component](https://primevue.org/togglebutton): ToggleButton is used to select a boolean value using a button.
- [Vue ToggleSwitch Component](https://primevue.org/toggleswitch): ToggleSwitch is used to select a boolean value.
- [Vue Toolbar Component](https://primevue.org/toolbar): Toolbar is a grouping component for buttons and other content.
- [Vue Tooltip Directive](https://primevue.org/tooltip): Tooltip directive provides advisory information for a component.
- [Vue Tree Component](https://primevue.org/tree): Tree is used to display hierarchical data.
- [Vue TreeSelect Component](https://primevue.org/treeselect): TreeSelect is a form component to choose from hierarchical data.
- [Vue TreeTable Component](https://primevue.org/treetable): TreeTable is used to display hierarchical data in tabular format.
- [Vue Virtual Scroller Component](https://primevue.org/virtualscroller): VirtualScroller is a performant approach to render large amounts of data efficiently.