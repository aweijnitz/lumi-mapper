import { config } from "@vue/test-utils";
import { defineComponent, h } from "vue";
import { vi } from "vitest";

class ResizeObserverMock {
  observe() {}
  unobserve() {}
  disconnect() {}
}

if (!("ResizeObserver" in globalThis)) {
  globalThis.ResizeObserver = ResizeObserverMock as typeof ResizeObserver;
}

class WritableMouseEvent extends Event {
  clientX = 0;
  clientY = 0;
  button = 0;
  pointerId = 0;
  deltaY = 0;
  offsetX = 0;
  offsetY = 0;
  pageX = 0;
  pageY = 0;

  constructor(type: string, props: Record<string, unknown> = {}) {
    super(type, {
      bubbles: Boolean(props.bubbles),
      cancelable: Boolean(props.cancelable),
      composed: Boolean(props.composed),
    });
    const assignable = [
      "clientX",
      "clientY",
      "button",
      "pointerId",
      "deltaY",
      "offsetX",
      "offsetY",
      "pageX",
      "pageY",
    ];
    for (const key of assignable) {
      if (key in props) {
        (this as Record<string, unknown>)[key] = props[key];
      }
    }
  }
}

if (globalThis.MouseEvent) {
  globalThis.MouseEvent = WritableMouseEvent as unknown as typeof MouseEvent;
}
if (!globalThis.PointerEvent) {
  globalThis.PointerEvent = WritableMouseEvent as unknown as typeof PointerEvent;
}
if (!globalThis.WheelEvent) {
  globalThis.WheelEvent = WritableMouseEvent as unknown as typeof WheelEvent;
}
if (globalThis.window) {
  globalThis.window.MouseEvent = globalThis.MouseEvent;
  globalThis.window.PointerEvent = globalThis.PointerEvent;
  globalThis.window.WheelEvent = globalThis.WheelEvent;
}

if (globalThis.Element?.prototype) {
  if (!globalThis.Element.prototype.setPointerCapture) {
    globalThis.Element.prototype.setPointerCapture = () => {};
  }
  if (!globalThis.Element.prototype.releasePointerCapture) {
    globalThis.Element.prototype.releasePointerCapture = () => {};
  }
}

if (globalThis.document?.createEvent) {
  const originalCreateEvent = globalThis.document.createEvent.bind(globalThis.document);
  globalThis.document.createEvent = ((type: string) => {
    if (["MouseEvent", "PointerEvent", "WheelEvent"].includes(type)) {
      return new WritableMouseEvent(type);
    }
    return originalCreateEvent(type);
  }) as typeof document.createEvent;
}

const makeInputStub = (tag: "input" | "textarea", toNumber = false) =>
  defineComponent({
    props: ["modelValue"],
    emits: ["update:modelValue"],
    setup(props, { attrs, emit }) {
      return () =>
        h(tag, {
          ...attrs,
          value: props.modelValue,
          onInput: (event: Event) => {
            const value = (event.target as HTMLInputElement).value;
            emit("update:modelValue", toNumber ? Number(value) : value);
          },
        });
    },
  });

const makeSelectStub = () =>
  defineComponent({
    props: ["modelValue", "options", "optionLabel", "optionValue"],
    emits: ["update:modelValue"],
    setup(props, { attrs, emit }) {
      return () =>
        h(
          "select",
          {
            ...attrs,
            value: props.modelValue as string,
            onChange: (event: Event) =>
              emit("update:modelValue", (event.target as HTMLSelectElement).value),
          },
          (props.options || []).map((option: Record<string, unknown>) =>
            h(
              "option",
              {
                value:
                  (props.optionValue && option[props.optionValue as string]) ??
                  option.value ??
                  option,
              },
              String(
                (props.optionLabel && option[props.optionLabel as string]) ??
                  option.label ??
                  option,
              ),
            ),
          ),
        );
    },
  });

const makeButtonStub = () =>
  defineComponent({
    props: ["label", "disabled", "loading"],
    emits: ["click"],
    setup(props, { attrs, emit, slots }) {
      return () =>
        h(
          "button",
          {
            ...attrs,
            disabled: props.disabled as boolean,
            onClick: () => emit("click"),
          },
          [slots.default?.(), props.label],
        );
    },
  });

const makeSlotStub = () =>
  defineComponent({
    setup(_props, { slots }) {
      return () => h("div", {}, slots.default?.());
    },
  });

const makeFileUploadStub = () =>
  defineComponent({
    emits: ["uploader"],
    setup(_props, { attrs, emit, slots }) {
      return () =>
        h(
          "button",
          {
            ...attrs,
            onClick: () =>
              emit("uploader", {
                files: [new File(["dummy"], "clip.mp4", { type: "video/mp4" })],
              }),
          },
          [slots.default?.(), "Upload"],
        );
    },
  });

vi.mock("primevue/inputtext", () => ({ default: makeInputStub("input") }));
vi.mock("primevue/textarea", () => ({ default: makeInputStub("textarea") }));
vi.mock("primevue/inputnumber", () => ({ default: makeInputStub("input", true) }));
vi.mock("primevue/dropdown", () => ({ default: makeSelectStub() }));
vi.mock("primevue/select", () => ({ default: makeSelectStub() }));
vi.mock("primevue/button", () => ({ default: makeButtonStub() }));
vi.mock("primevue/datatable", () => ({ default: makeSlotStub() }));
vi.mock("primevue/column", () => ({ default: makeSlotStub() }));
vi.mock("primevue/message", () => ({ default: makeSlotStub() }));
vi.mock("primevue/fileupload", () => ({ default: makeFileUploadStub() }));
vi.mock("primevue/speeddial", () => ({ default: makeSlotStub() }));

config.global.mocks.$primevue = { config: {} };
config.global.stubs = {
  InputText: {
    props: ["modelValue"],
    template:
      "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
  },
  Textarea: {
    props: ["modelValue"],
    template:
      "<textarea v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', $event.target.value)\" />",
  },
  InputNumber: {
    props: ["modelValue"],
    template:
      "<input v-bind=\"$attrs\" :value=\"modelValue\" @input=\"$emit('update:modelValue', Number($event.target.value))\" />",
  },
  Dropdown: {
    props: ["modelValue", "options", "optionLabel", "optionValue"],
    template: `
      <select v-bind="$attrs" :value="modelValue" @change="$emit('update:modelValue', $event.target.value)">
        <option
          v-for="option in options"
          :key="option[optionValue] ?? option.value ?? option"
          :value="option[optionValue] ?? option.value ?? option"
        >
          {{ option[optionLabel] ?? option.label ?? option }}
        </option>
      </select>
    `,
  },
  Select: {
    props: ["modelValue", "options", "optionLabel", "optionValue"],
    template: `
      <select v-bind="$attrs" :value="modelValue" @change="$emit('update:modelValue', $event.target.value)">
        <option
          v-for="option in options"
          :key="option[optionValue] ?? option.value ?? option"
          :value="option[optionValue] ?? option.value ?? option"
        >
          {{ option[optionLabel] ?? option.label ?? option }}
        </option>
      </select>
    `,
  },
  Button: {
    props: ["label", "disabled", "loading"],
    template:
      "<button v-bind=\"$attrs\" :disabled=\"disabled\" @click=\"$emit('click')\"><slot />{{ label }}</button>",
  },
  DataTable: {
    template: "<div><slot /></div>",
  },
  Column: {
    template: "<div><slot /></div>",
  },
  Message: {
    template: "<div><slot /></div>",
  },
  FileUpload: {
    template:
      "<button v-bind=\"$attrs\" @click=\"$emit('uploader', { files: [] })\"><slot />Upload</button>",
  },
  SpeedDial: {
    template: "<div></div>",
  },
  SectionHeader: {
    template: "<div></div>",
  },
};
