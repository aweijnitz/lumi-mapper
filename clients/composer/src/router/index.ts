import { createRouter, createWebHistory } from "vue-router";
import ComposerView from "../views/ComposerView.vue";

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: "/",
      name: "composer",
      component: ComposerView,
    },
  ],
});

export default router;
