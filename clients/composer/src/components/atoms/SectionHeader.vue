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
  gap: 4px;
  margin-bottom: 12px;
}

.section-header__title {
  font-size: 0.9rem;
  text-transform: uppercase;
  letter-spacing: 0.12em;
  font-weight: 700;
}

.section-header__subtitle {
  color: #4a4640;
  font-size: 0.85rem;
}
</style>
