import { expect, it } from "vitest";
import { createId } from "./useIds";

it("createId prefixes random ids", () => {
  const id = createId("project");
  expect(id.startsWith("project-")).toBe(true);
});
