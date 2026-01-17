export type ApiError = {
  message: string;
  status?: number;
};

const apiBase = import.meta.env.VITE_API_BASE ?? "";

const resolveUrl = (url: string) => {
  if (!apiBase) {
    return url;
  }
  return new URL(url, apiBase).toString();
};

const parseErrorMessage = async (response: Response) => {
  const text = await response.text();
  if (!text) {
    return "Request failed.";
  }

  try {
    const parsed = JSON.parse(text) as { error?: string };
    return parsed.error ?? text;
  } catch {
    return text;
  }
};

export const requestJson = async <T>(url: string, options: RequestInit) => {
  const response = await fetch(resolveUrl(url), {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers ?? {}),
    },
  });

  if (!response.ok) {
    const message = await parseErrorMessage(response);
    const error: ApiError = { message, status: response.status };
    throw error;
  }

  if (response.status === 204) {
    return null as T;
  }

  return (await response.json()) as T;
};

export const requestFormData = async <T>(url: string, options: RequestInit) => {
  const response = await fetch(resolveUrl(url), options);

  if (!response.ok) {
    const message = await parseErrorMessage(response);
    const error: ApiError = { message, status: response.status };
    throw error;
  }

  if (response.status === 204) {
    return null as T;
  }

  const text = await response.text();
  if (!text) {
    return null as T;
  }

  try {
    return JSON.parse(text) as T;
  } catch {
    return text as T;
  }
};

export const resolveErrorMessage = (error: unknown, fallback: string) => {
  if (error instanceof Error) {
    return error.message;
  }
  if (error && typeof error === "object" && "message" in error) {
    return String((error as ApiError).message);
  }
  return fallback;
};
