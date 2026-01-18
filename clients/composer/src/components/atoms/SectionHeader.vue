<script setup lang="ts">
defineProps<{ title: string; subtitle?: string }>();
</script>

<template>
  <div class="section-header">
    <div class="section-header__title">{{ title }}</div>
    <div v-if="subtitle" class="section-header__subtitle">{{ subtitle }}</div>
  </div>
</template>

<style scoped>
.section-header {
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin-bottom: 8px;
  padding-bottom: 6px;
  border-bottom: 1px solid #2a2a2a;
}

.section-header__title {
  font-size: 10px;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  font-weight: 500;
  color: #888;
}

.section-header__subtitle {
  color: #555;
  font-size: 10px;
  line-height: 1.4;
}
</style>
