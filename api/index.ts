import { spawnSync } from 'child_process';
import { existsSync, mkdirSync } from 'fs';
import http, { IncomingMessage, ServerResponse } from 'http';
import path from 'path';
import { URL } from 'url';

interface ColumnRequest {
  name: string;
  type: string;
  nullable?: boolean;
  length?: number;
}

interface CreateTableRequest {
  name: string;
  columns: ColumnRequest[];
}

interface RowInsertRequest {
  values: Record<string, string | number | null>;
}

interface FilterRequest {
  column: string;
  op?: 'eq' | 'neq' | 'lt' | 'lte' | 'gt' | 'gte';
  value: string | number;
}

interface SqlRequest {
  sql: string;
}

const repoRoot: string = existsSync(path.resolve(process.cwd(), 'build'))
  ? process.cwd()
  : path.resolve(process.cwd(), '..');

const dbDataDir: string = path.resolve(repoRoot, 'data');
mkdirSync(dbDataDir, { recursive: true });

const dbPath: string = process.env.DBCORE_DB_PATH ?? path.resolve(dbDataDir, 'advanced.db');

function resolveEngineBinary(): string | undefined {
  const preferredBinaryName: string = process.env.DBCORE_BINARY ?? 'dbcore_engine';
  const candidatePaths: string[] = [
    path.resolve(repoRoot, 'build', 'engine', `${preferredBinaryName}.exe`),
    path.resolve(repoRoot, 'build', 'engine', preferredBinaryName)
  ];

  return candidatePaths.find((candidate) => existsSync(candidate));
}

function sendJson(res: ServerResponse, statusCode: number, body: unknown): void {
  const responseBody = JSON.stringify(body);
  res.writeHead(statusCode, {
    'Content-Type': 'application/json',
    'Content-Length': Buffer.byteLength(responseBody)
  });
  res.end(responseBody);
}

function parseJsonBody<T>(req: IncomingMessage): Promise<T> {
  return new Promise((resolve, reject) => {
    let raw = '';
    req.on('data', (chunk: Buffer) => {
      raw += chunk.toString('utf-8');
    });

    req.on('end', () => {
      try {
        resolve(JSON.parse(raw) as T);
      } catch (error) {
        reject(error);
      }
    });

    req.on('error', reject);
  });
}

function buildColumnSpec(columns: ColumnRequest[]): string {
  return columns
    .map((column) => {
      const normalizedType = column.type.toLowerCase();
      if (normalizedType === 'varchar') {
        const length = column.length ?? 255;
        return `${column.name}:varchar(${length})${column.nullable ? '?' : ''}`;
      }

      return `${column.name}:${normalizedType}${column.nullable ? '?' : ''}`;
    })
    .join(',');
}

function formatScalar(value: string | number | null): string {
  if (value === null) {
    return 'null';
  }

  if (typeof value === 'number') {
    return String(value);
  }

  return `'${value.replace(/'/g, "\\'")}'`;
}

function buildRowSpec(values: Record<string, string | number | null>): string {
  return Object.entries(values)
    .map(([column, value]) => `${column}=${formatScalar(value)}`)
    .join(',');
}

function buildFilterSpec(filters: FilterRequest[]): string {
  return filters
    .map((filter) => `${filter.column}:${filter.op ?? 'eq'}:${formatScalar(filter.value)}`)
    .join(',');
}

function runEngineCommand(args: string[]): { status: number; stdout: string; stderr: string } {
  const binaryPath = resolveEngineBinary();
  if (!binaryPath) {
    return {
      status: 1,
      stdout: '',
      stderr: 'C++ engine binary not found. Build using: cmake --build build --target dbcore_engine'
    };
  }

  const result = spawnSync(binaryPath, args, { encoding: 'utf-8' });
  return {
    status: result.status ?? 1,
    stdout: result.stdout ?? '',
    stderr: result.stderr ?? ''
  };
}

function parseEngineJson(stdout: string): unknown {
  const trimmed = stdout.trim();
  if (trimmed.length === 0) {
    return { ok: false, error: 'Empty response from C++ engine' };
  }

  try {
    return JSON.parse(trimmed);
  } catch {
    return { ok: false, error: `Invalid JSON from C++ engine: ${trimmed}` };
  }
}

async function requestHandler(req: IncomingMessage, res: ServerResponse): Promise<void> {
  const method = req.method ?? 'GET';
  const requestUrl = new URL(req.url ?? '/', 'http://localhost');

  if (method === 'GET' && requestUrl.pathname === '/health') {
    sendJson(res, 200, { ok: true, service: 'advanced-database-api' });
    return;
  }

  if (method === 'POST' && requestUrl.pathname === '/init') {
    const result = runEngineCommand(['init', dbPath]);
    const payload = parseEngineJson(result.stdout);
    sendJson(res, result.status === 0 ? 200 : 500, payload);
    return;
  }

  if (method === 'POST' && requestUrl.pathname === '/sql') {
    try {
      const body = await parseJsonBody<SqlRequest>(req);
      if (!body.sql || body.sql.trim().length === 0) {
        sendJson(res, 400, { ok: false, error: 'SQL statement is required' });
        return;
      }

      const result = runEngineCommand(['sql', dbPath, body.sql]);
      const payload = parseEngineJson(result.stdout);
      sendJson(res, result.status === 0 ? 200 : 400, payload);
    } catch {
      sendJson(res, 400, { ok: false, error: 'Invalid JSON body' });
    }
    return;
  }

  if (method === 'POST' && requestUrl.pathname === '/tables') {
    try {
      const body = await parseJsonBody<CreateTableRequest>(req);
      const columnSpec = buildColumnSpec(body.columns);
      const result = runEngineCommand(['create_table', dbPath, body.name, columnSpec]);
      const payload = parseEngineJson(result.stdout);
      sendJson(res, result.status === 0 ? 200 : 400, payload);
    } catch {
      sendJson(res, 400, { ok: false, error: 'Invalid JSON body' });
    }
    return;
  }

  if (method === 'GET' && requestUrl.pathname === '/tables') {
    const result = runEngineCommand(['list_tables', dbPath]);
    const payload = parseEngineJson(result.stdout);
    sendJson(res, result.status === 0 ? 200 : 500, payload);
    return;
  }

  if (method === 'GET' && requestUrl.pathname.startsWith('/tables/')) {
    const pathParts = requestUrl.pathname.split('/').filter(Boolean);
    const tableName = decodeURIComponent(pathParts[1] ?? '');

    if (pathParts.length === 2) {
      const result = runEngineCommand(['describe_table', dbPath, tableName]);
      const payload = parseEngineJson(result.stdout);
      sendJson(res, result.status === 0 ? 200 : 404, payload);
      return;
    }

    if (pathParts.length === 3 && pathParts[2] === 'rows') {
      const filterColumn = requestUrl.searchParams.get('column');
      const filterValue = requestUrl.searchParams.get('value');
      const filterOp = (requestUrl.searchParams.get('op') as FilterRequest['op'] | null) ?? 'eq';

      const args = ['select', dbPath, tableName];
      if (filterColumn && filterValue !== null) {
        args.push(buildFilterSpec([{ column: filterColumn, op: filterOp, value: filterValue }]));
      }

      const result = runEngineCommand(args);
      const payload = parseEngineJson(result.stdout);
      sendJson(res, result.status === 0 ? 200 : 400, payload);
      return;
    }

    sendJson(res, 404, { ok: false, error: 'Route not found' });
    return;
  }

  if (method === 'POST' && requestUrl.pathname.startsWith('/tables/')) {
    const pathParts = requestUrl.pathname.split('/').filter(Boolean);
    const tableName = decodeURIComponent(pathParts[1] ?? '');

    if (pathParts.length === 3 && pathParts[2] === 'rows') {
      try {
        const body = await parseJsonBody<RowInsertRequest>(req);
        const rowSpec = buildRowSpec(body.values);
        const result = runEngineCommand(['insert', dbPath, tableName, rowSpec]);
        const payload = parseEngineJson(result.stdout);
        sendJson(res, result.status === 0 ? 200 : 400, payload);
      } catch {
        sendJson(res, 400, { ok: false, error: 'Invalid JSON body' });
      }
      return;
    }

    sendJson(res, 404, { ok: false, error: 'Route not found' });
    return;
  }

  sendJson(res, 404, { ok: false, error: 'Route not found' });
}

const port = Number(process.env.PORT ?? 3000);
const server = http.createServer((req, res) => {
  void requestHandler(req, res);
});

server.listen(port, () => {
  console.log(`Advanced Database API listening on http://localhost:${port}`);
  console.log(`Database file path: ${dbPath}`);
});