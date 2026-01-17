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
  </div>
</template>

<style scoped>
.renderer-browser {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.renderer-browser__toolbar {
  display: flex;
  justify-content: flex-end;
}

.renderer-browser__list {
  margin: 0;
  padding-left: 18px;
  color: #cbbfad;
;
}

.renderer-browser__empty {
  color: #cbbfad;
;
}

.renderer-browser__message {
  margin: 0;
}
</style>
