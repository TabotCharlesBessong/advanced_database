export interface ConnectionConfig {
  baseUrl: string;
  username: string;
  password: string;
}

export interface LoginResponse {
  token: string;
  expiresIn: string;
  user: {
    username: string;
  };
}

export interface ApiResult<T> {
  ok: boolean;
  data?: T;
  error?: string;
}

const DEFAULT_TIMEOUT_MS = 10000;

async function apiFetch<T>(
  url: string,
  init: RequestInit,
  timeoutMs = DEFAULT_TIMEOUT_MS
): Promise<ApiResult<T>> {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch(url, { ...init, signal: controller.signal });
    const data = await response.json().catch(() => ({}));

    if (!response.ok) {
      const error = typeof data?.error === 'string' ? data.error : `HTTP ${response.status}`;
      return { ok: false, error };
    }

    return { ok: true, data: data as T };
  } catch (error) {
    if (error instanceof DOMException && error.name === 'AbortError') {
      return { ok: false, error: 'Request timed out. Please check the API URL.' };
    }
    return { ok: false, error: error instanceof Error ? error.message : 'Network error' };
  } finally {
    clearTimeout(timeout);
  }
}

export async function login(config: ConnectionConfig): Promise<ApiResult<LoginResponse>> {
  return apiFetch<LoginResponse>(`${config.baseUrl}/auth/login`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      username: config.username,
      password: config.password,
    }),
  });
}

export async function initializeDatabase(baseUrl: string, token: string): Promise<ApiResult<{ ok: boolean }>> {
  return apiFetch<{ ok: boolean }>(`${baseUrl}/init`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${token}`,
    },
  });
}

export async function executeSql(
  baseUrl: string,
  token: string,
  sql: string
): Promise<ApiResult<unknown>> {
  return apiFetch<unknown>(`${baseUrl}/sql`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Authorization: `Bearer ${token}`,
    },
    body: JSON.stringify({ sql }),
  });
}
