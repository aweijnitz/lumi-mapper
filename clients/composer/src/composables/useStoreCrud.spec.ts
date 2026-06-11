import { expect, it, vi } from "vitest";
import {
  appendEntity,
  clearActiveEntity,
  removeEntity,
  replaceActiveEntity,
  replaceEntity,
  runStoreRequest,
} from "./useStoreCrud";

it("runStoreRequest returns task result and resets loading", async () => {
  const store = {
    isLoading: false,
    error: "old error",
  };

  const result = await runStoreRequest(store, "Failed.", async () => "ok");

  expect(result).toBe("ok");
  expect(store.isLoading).toBe(false);
  expect(store.error).toBeNull();
});

it("runStoreRequest stores error without throwing by default", async () => {
  const store = {
    isLoading: false,
    error: null as string | null,
  };

  const result = await runStoreRequest(store, "Fallback.", async () => {
    throw { message: "Nope" };
  });

  expect(result).toBeUndefined();
  expect(store.isLoading).toBe(false);
  expect(store.error).toBe("Nope");
});

it("runStoreRequest rethrows when requested", async () => {
  const store = {
    isLoading: false,
    error: null as string | null,
  };
  const error = new Error("Boom");

  await expect(
    runStoreRequest(
      store,
      "Fallback.",
      async () => {
        throw error;
      },
      { rethrow: true },
    ),
  ).rejects.toBe(error);
  expect(store.error).toBe("Boom");
  expect(store.isLoading).toBe(false);
});

it("entity helpers append, replace, and clear selections", () => {
  const one = { id: "1", name: "one" };
  const two = { id: "2", name: "two" };
  const twoUpdated = { id: "2", name: "two-updated" };

  const appended = appendEntity([one], two);
  const replaced = replaceEntity(appended, twoUpdated);
  const removed = removeEntity(replaced, "1");

  expect(appended).toEqual([one, two]);
  expect(replaced).toEqual([one, twoUpdated]);
  expect(removed).toEqual([twoUpdated]);
  expect(replaceActiveEntity(two, twoUpdated)).toEqual(twoUpdated);
  expect(replaceActiveEntity(one, twoUpdated)).toEqual(one);
  expect(clearActiveEntity(twoUpdated, "2")).toBeNull();
  expect(clearActiveEntity(one, "2")).toEqual(one);
});
