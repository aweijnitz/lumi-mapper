<script setup lang="ts">
import { computed, onMounted } from "vue";
import { storeToRefs } from "pinia";
import Button from "primevue/button";
import Message from "primevue/message";
import { useRendererStore } from "../../stores/rendererStore";

const rendererStore = useRendererStore();
const { lastStatus, error } = storeToRefs(rendererStore);

const renderers = computed(() => lastStatus.value?.renderers ?? []);

onMounted(() => {
  rendererStore.ping();
});
</script>

<template>
  <div class="renderer-browser">
    <div class="renderer-browser__toolbar">
      <Button
        label="Refresh"
        icon="pi pi-refresh"
        text
        :loading="rendererStore.isLoading"
        data-testid="renderer-refresh"
        @click="rendererStore.ping"
      />
    </div>

    <Message v-if="error" severity="error" class="renderer-browser__message">
      {{ error }}
    </Message>

    <div v-if="renderers.length === 0" class="renderer-browser__empty">
      No renderers connected.
    </div>

    <ul v-else class="renderer-browser__list">
      <li v-for="renderer in renderers" :key="renderer">{{ renderer }}</li>
    </ul>

    <div class="renderer-browser__shortcuts">
      <div class="renderer-browser__shortcuts-title">Renderer Shortcuts</div>
      <div class="renderer-browser__shortcut"><kbd>G</kbd> Cycle grid (off/solid/overlay)</div>
      <div class="renderer-browser__shortcut"><kbd>M</kbd> Toggle monochrome filter</div>
      <div class="renderer-browser__shortcut"><kbd>T</kbd> Toggle color tint</div>
      <div class="renderer-browser__shortcut"><kbd>D</kbd> Toggle dramatic mode</div>
      <div class="renderer-browser__shortcut"><kbd>1-5</kbd> Switch color palette</div>
      <div class="renderer-browser__shortcut"><kbd>I</kbd> Toggle debug info</div>
    </div>
  </div>
</template>

<style scoped>
.renderer-browser {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.renderer-browser__toolbar {
  display: flex;
  justify-content: flex-end;
  padding-bottom: 8px;
  border-bottom: 1px solid #2a2a2a;
}

.renderer-browser__list {
  margin: 0;
  padding-left: 16px;
  color: #aaa;
  font-size: 12px;
  line-height: 1.6;
}

.renderer-browser__list li {
  padding: 2px 0;
}

.renderer-browser__empty {
  color: #555;
  font-size: 12px;
  padding: 12px 0;
  text-align: center;
  font-style: italic;
}

.renderer-browser__message {
  margin: 0;
}

.renderer-browser__shortcuts {
  margin-top: 16px;
  padding-top: 12px;
  border-top: 1px solid #2a2a2a;
}

.renderer-browser__shortcuts-title {
  font-size: 10px;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: #00b4d8;
  margin-bottom: 8px;
}

.renderer-browser__shortcut {
  font-size: 11px;
  color: #888;
  padding: 3px 0;
  display: flex;
  align-items: center;
  gap: 8px;
}

.renderer-browser__shortcut kbd {
  display: inline-block;
  min-width: 24px;
  padding: 2px 6px;
  font-family: monospace;
  font-size: 10px;
  color: #ccc;
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 3px;
  text-align: center;
}
</style>
