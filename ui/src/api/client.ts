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

export interface SignupResponse extends LoginResponse {
  message: string;
}

export interface ApiResult<T> {
  ok: boolean;
  data?: T;
  error?: string;
}

export interface TableColumn {
  name: string;
  type: string;
  nullable: boolean;
}

export interface TableSchema {
  name: string;
  columns: TableColumn[];
}

export interface ListTablesResponse {
  ok: boolean;
  tables: string[];
}

export interface DescribeTableResponse {
  ok: boolean;
  table: TableSchema;
}

export interface RowsResponse {
  ok: boolean;
  rows: Array<Record<string, string | number | null>>;
}

export interface InsertRowResponse {
  ok: boolean;
  table: string;
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

export async function signup(config: ConnectionConfig): Promise<ApiResult<SignupResponse>> {
  return apiFetch<SignupResponse>(`${config.baseUrl}/auth/signup`, {
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

export async function listTables(
  baseUrl: string,
  token: string
): Promise<ApiResult<ListTablesResponse>> {
  return apiFetch<ListTablesResponse>(`${baseUrl}/tables`, {
    method: 'GET',
    headers: {
      Authorization: `Bearer ${token}`,
    },
  });
}

export async function describeTable(
  baseUrl: string,
  token: string,
  tableName: string
): Promise<ApiResult<DescribeTableResponse>> {
  return apiFetch<DescribeTableResponse>(
    `${baseUrl}/tables/${encodeURIComponent(tableName)}`,
    {
      method: 'GET',
      headers: {
        Authorization: `Bearer ${token}`,
      },
    }
  );
}

export async function getTableRows(
  baseUrl: string,
  token: string,
  tableName: string
): Promise<ApiResult<RowsResponse>> {
  return apiFetch<RowsResponse>(
    `${baseUrl}/tables/${encodeURIComponent(tableName)}/rows`,
    {
      method: 'GET',
      headers: {
        Authorization: `Bearer ${token}`,
      },
    }
  );
}

export async function insertTableRow(
  baseUrl: string,
  token: string,
  tableName: string,
  values: Record<string, string | number | null>
): Promise<ApiResult<InsertRowResponse>> {
  return apiFetch<InsertRowResponse>(
    `${baseUrl}/tables/${encodeURIComponent(tableName)}/rows`,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${token}`,
      },
      body: JSON.stringify({ values }),
    }
  );
}
