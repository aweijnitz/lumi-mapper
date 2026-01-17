import { defineStore } from "pinia";
import type { Asset } from "../types/asset";
import { requestFormData, requestJson, resolveErrorMessage } from "../composables/useApiClient";

export const useAssetStore = defineStore("assets", {
  state: () => ({
    assets: [] as Asset[],
    activeAsset: null as Asset | null,
    isLoading: false,
    error: null as string | null,
  }),
  actions: {
    async fetchAssets() {
      this.isLoading = true;
      this.error = null;
      try {
        const assets = await requestJson<Asset[]>("/api/assets", { method: "GET" });
        this.assets = assets ?? [];
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to load assets.");
      } finally {
        this.isLoading = false;
      }
    },
    async uploadAsset(file: File) {
      this.isLoading = true;
      this.error = null;
      try {
        const formData = new FormData();
        formData.append("file", file);
        await requestFormData("/api/assets", {
          method: "POST",
          body: formData,
        });
        await this.fetchAssets();
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to upload asset.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    async deleteAsset(assetId: string) {
      this.isLoading = true;
      this.error = null;
      try {
        await requestJson(`/api/assets/${assetId}`, {
          method: "DELETE",
        });
        this.assets = this.assets.filter((asset) => asset.id !== assetId);
        if (this.activeAsset?.id === assetId) {
          this.activeAsset = null;
        }
      } catch (error) {
        this.error = resolveErrorMessage(error, "Failed to delete asset.");
        throw error;
      } finally {
        this.isLoading = false;
      }
    },
    setActiveAsset(asset: Asset | null) {
      this.activeAsset = asset;
    },
  },
});
