import { resolveErrorMessage } from "./useApiClient";

type AsyncStoreState = {
  isLoading: boolean;
  error: string | null;
};

type Identifiable = {
  id: string;
};

type StoreRequestOptions = {
  rethrow?: boolean;
};

export async function runStoreRequest<T>(
  store: AsyncStoreState,
  fallbackMessage: string,
  task: () => Promise<T>,
  options: StoreRequestOptions & { rethrow: true },
): Promise<T>;
export async function runStoreRequest<T>(
  store: AsyncStoreState,
  fallbackMessage: string,
  task: () => Promise<T>,
  options?: StoreRequestOptions,
): Promise<T | undefined>;
export async function runStoreRequest<T>(
  store: AsyncStoreState,
  fallbackMessage: string,
  task: () => Promise<T>,
  options: StoreRequestOptions = {},
): Promise<T | undefined> {
  store.isLoading = true;
  store.error = null;

  try {
    return await task();
  } catch (error) {
    store.error = resolveErrorMessage(error, fallbackMessage);
    if (options.rethrow) {
      throw error;
    }
    return undefined;
  } finally {
    store.isLoading = false;
  }
}

export const appendEntity = <T>(entities: readonly T[], entity: T) => [...entities, entity];

export const replaceEntity = <T extends Identifiable>(entities: readonly T[], entity: T) =>
  entities.map((candidate) => (candidate.id === entity.id ? entity : candidate));

export const removeEntity = <T extends Identifiable>(entities: readonly T[], entityId: string) =>
  entities.filter((candidate) => candidate.id !== entityId);

export const replaceActiveEntity = <T extends Identifiable>(activeEntity: T | null, entity: T) =>
  activeEntity?.id === entity.id ? entity : activeEntity;

export const clearActiveEntity = <T extends Identifiable>(
  activeEntity: T | null,
  entityId: string,
) => (activeEntity?.id === entityId ? null : activeEntity);
