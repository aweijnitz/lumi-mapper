<script setup lang="ts">
import { computed } from "vue";
import Button from "primevue/button";
import Message from "primevue/message";
import SectionHeader from "../atoms/SectionHeader.vue";
import { useProjectStore } from "../../stores/projectStore";
import { useCueStore } from "../../stores/cueStore";
import { useRendererStore } from "../../stores/rendererStore";

const projectStore = useProjectStore();
const cueStore = useCueStore();
const rendererStore = useRendererStore();

const canPlay = computed(
  () => Boolean(projectStore.activeProject && cueStore.activeCue && !rendererStore.isLoading),
);

const playCue = async () => {
  if (!projectStore.activeProject || !cueStore.activeCue) {
    return;
  }
  await rendererStore.playCue(projectStore.activeProject.id, cueStore.activeCue.id);
};
</script>

<template>
  <section class="playback-section">
    <SectionHeader title="Playback" subtitle="Send cues to the renderer." />

    <div class="playback-section__status">
      <div>
        <strong>Active cue:</strong>
        {{ cueStore.activeCue ? cueStore.activeCue.name : "None" }}
      </div>
      <div>
        <strong>Renderer:</strong>
        {{ rendererStore.lastStatus ? "Connected" : "Unknown" }}
      </div>
    </div>

    <div class="playback-section__actions">
      <Button label="Ping Renderer" icon="pi pi-bolt" text @click="rendererStore.ping" />
      <Button label="Play Cue" icon="pi pi-play" :disabled="!canPlay" @click="playCue" />
    </div>

    <Message v-if="rendererStore.error" severity="error" class="playback-section__message">
      {{ rendererStore.error }}
    </Message>
  </section>
</template>

<style scoped>
.playback-section {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.playback-section__status {
  display: grid;
  gap: 6px;
  color: #4a4640;
}

.playback-section__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}
</style>
