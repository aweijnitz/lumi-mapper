import type { Project } from "../types/project";

const resolveTimestamp = (value: string | undefined, fallback: string) => {
  if (value && value.trim().length > 0) {
    return value;
  }

  return fallback;
};

export const normalizeProjectPayload = (
  payload: Project,
  now = new Date().toISOString(),
): Project => ({
  ...payload,
  createdAt: resolveTimestamp(payload.createdAt, now),
  updatedAt: resolveTimestamp(payload.updatedAt, now),
  assetIds: Array.isArray(payload.assetIds) ? payload.assetIds : [],
  sceneIds: Array.isArray(payload.sceneIds) ? payload.sceneIds : [],
  feedIds: Array.isArray(payload.feedIds) ? payload.feedIds : [],
  cueOrder: Array.isArray(payload.cueOrder) ? payload.cueOrder : [],
  settings: {
    controllers: payload.settings?.controllers ?? {},
    midiChannels: Array.isArray(payload.settings?.midiChannels) ? payload.settings.midiChannels : [],
    globalConfig: payload.settings?.globalConfig ?? {},
  },
});
