import { defineStore } from "pinia";
import type { Asset } from "../types/asset";
import { requestFormData, requestJson } from "../composables/useApiClient";
import {
  clearActiveEntity,
  removeEntity,
  runStoreRequest,
} from "../composables/useStoreCrud";

export const useAssetStore = defineStore("assets", {
  state: () => ({
    assets: [] as Asset[],
    activeAsset: null as Asset | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchAssets() {
      const assets = await runStoreRequest(this, "Failed to load assets.", async () => {
        const assets = await requestJson<Asset[]>("/api/assets", { method: "GET" });
        return assets ?? [];
      });
      if (assets !== undefined) {
        this.assets = assets;
      }
    },
    async uploadAsset(file: File) {
      await runStoreRequest(
        this,
        "Failed to upload asset.",
        async () => {
          const formData = new FormData();
          formData.append("file", file);
          await requestFormData("/api/assets", {
            method: "POST",
            body: formData,
          });
          await this.fetchAssets();
        },
        { rethrow: true },
      );
    },
    async deleteAsset(assetId: string) {
      await runStoreRequest(
        this,
        "Failed to delete asset.",
        async () => {
          await requestJson(`/api/assets/${assetId}`, {
            method: "DELETE",
          });
          this.assets = removeEntity(this.assets, assetId);
          this.activeAsset = clearActiveEntity(this.activeAsset, assetId);
        },
        { rethrow: true },
      );
    },
    setActiveAsset(asset: Asset | null) {
      this.activeAsset = asset;
    },
  },
});
