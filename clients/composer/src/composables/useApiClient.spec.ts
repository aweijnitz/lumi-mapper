import { expect, it, vi } from "vitest";
import { requestJson } from "./useApiClient";

const mockFetch = (status: number, body: unknown) => {
  return vi.fn().mockResolvedValue({
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(typeof body === "string" ? body : JSON.stringify(body)),
    json: vi.fn().mockResolvedValue(body),
  });
};

it("requestJson returns parsed JSON on success", async () => {
  globalThis.fetch = mockFetch(200, { ok: true }) as unknown as typeof fetch;

  const result = await requestJson<{ ok: boolean }>("/api/test", { method: "GET" });
  expect(result.ok).toBe(true);
});

it("requestJson throws on error status", async () => {
  globalThis.fetch = mockFetch(400, { error: "Bad Request" }) as unknown as typeof fetch;

  await expect(requestJson("/api/test", { method: "GET" })).rejects.toMatchObject({
    message: "Bad Request",
    status: 400,
  });
});
