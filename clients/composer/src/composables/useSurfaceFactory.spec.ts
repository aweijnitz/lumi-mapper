import { describe, expect, it } from "vitest";
import { createSurface } from "./useSurfaceFactory";

describe("createSurface", () => {
  it("creates a rectangle surface with defaults", () => {
    const surface = createSurface("rectangle", { feedId: "feed-1", index: 2 });

    expect(surface.id).toMatch(/^surface-/);
    expect(surface.name).toBe("Rectangle 2");
    expect(surface.feedId).toBe("feed-1");
    expect(surface.vertices).toHaveLength(4);
    expect(surface.vertices[0]).toEqual({ x: -0.5, y: -0.5 });
    expect(surface.opacity).toBe(1);
    expect(surface.brightness).toBe(1);
    expect(surface.blendMode).toBe("Normal");
  });

  it("creates a quad surface with a custom name", () => {
    const surface = createSurface("quad", { feedId: "feed-2", name: "Warped", zOrder: 3 });

    expect(surface.name).toBe("Warped");
    expect(surface.feedId).toBe("feed-2");
    expect(surface.zOrder).toBe(3);
    expect(surface.vertices).toHaveLength(4);
  });
});
