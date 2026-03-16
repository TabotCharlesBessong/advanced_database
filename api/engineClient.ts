import { spawnSync } from 'child_process';
import { existsSync, mkdirSync } from 'fs';
import path from 'path';

export interface ColumnRequest {
  name: string;
  type: string;
  nullable?: boolean;
  length?: number;
}

export interface FilterRequest {
  column: string;
  op?: 'eq' | 'neq' | 'lt' | 'lte' | 'gt' | 'gte';
  value: string | number;
}

export interface EngineCommandResult {
  status: number;
  body: unknown;
}

export interface EngineClient {
  init(): EngineCommandResult;
  executeSql(sql: string): EngineCommandResult;
  createTable(name: string, columns: ColumnRequest[]): EngineCommandResult;
  listTables(): EngineCommandResult;
  describeTable(name: string): EngineCommandResult;
  selectRows(name: string, filters?: FilterRequest[]): EngineCommandResult;
  insertRow(name: string, values: Record<string, string | number | null>): EngineCommandResult;
}

interface EngineClientOptions {
  repoRoot?: string;
  dbPath?: string;
}

function resolveRepoRoot(): string {
  return existsSync(path.resolve(process.cwd(), 'build'))
    ? process.cwd()
    : path.resolve(process.cwd(), '..');
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

function resolveEngineBinary(repoRoot: string): string | undefined {
  const preferredBinaryName = process.env.DBCORE_BINARY ?? 'dbcore_engine';
  const candidatePaths = [
    path.resolve(repoRoot, 'build', 'engine', `${preferredBinaryName}.exe`),
    path.resolve(repoRoot, 'build', 'engine', preferredBinaryName)
  ];

  return candidatePaths.find((candidate) => existsSync(candidate));
}

function parseEngineBody(stdout: string): unknown {
  const trimmed = stdout.trim();
  if (trimmed.length === 0) {
    return { ok: false, error: 'Empty response from C++ engine' };
  }

  try {
    return JSON.parse(trimmed);
  } catch {
    // Some engine commands can print informational lines before the JSON payload.
    // Try to parse the trailing section(s) as JSON before surfacing an error.
    const lines = trimmed
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.length > 0);

    for (let i = 0; i < lines.length; i += 1) {
      const candidate = lines.slice(i).join('\n');
      try {
        return JSON.parse(candidate);
      } catch {
        // Continue trying smaller leading trims.
      }
    }

    const firstBrace = trimmed.indexOf('{');
    const lastBrace = trimmed.lastIndexOf('}');
    if (firstBrace !== -1 && lastBrace > firstBrace) {
      const candidate = trimmed.slice(firstBrace, lastBrace + 1);
      try {
        return JSON.parse(candidate);
      } catch {
        // Fall through to the original error for debugging visibility.
      }
    }

    return { ok: false, error: `Invalid JSON from C++ engine: ${trimmed}` };
  }
}

export function createEngineClient(options: EngineClientOptions = {}): EngineClient {
  const repoRoot = options.repoRoot ?? resolveRepoRoot();
  const dbDataDir = path.resolve(repoRoot, 'data');
  mkdirSync(dbDataDir, { recursive: true });
  const dbPath = options.dbPath ?? process.env.DBCORE_DB_PATH ?? path.resolve(dbDataDir, 'advanced.db');

  const runEngineCommand = (args: string[]): EngineCommandResult => {
    const binaryPath = resolveEngineBinary(repoRoot);
    if (!binaryPath) {
      return {
        status: 500,
        body: {
          ok: false,
          error: 'C++ engine binary not found. Build using: cmake --build build --target dbcore_engine'
        }
      };
    }

    const result = spawnSync(binaryPath, args, { encoding: 'utf-8' });
    const status = result.status === 0 ? 200 : 400;
    const parsedBody = parseEngineBody(result.stdout ?? '');
    const stderr = (result.stderr ?? '').trim();

    if (stderr.length > 0 && typeof parsedBody === 'object' && parsedBody !== null && !Array.isArray(parsedBody)) {
      return {
        status,
        body: {
          ...parsedBody,
          stderr
        }
      };
    }

    return { status, body: parsedBody };
  };

  return {
    init(): EngineCommandResult {
      return runEngineCommand(['init', dbPath]);
    },
    executeSql(sql: string): EngineCommandResult {
      return runEngineCommand(['sql', dbPath, sql]);
    },
    createTable(name: string, columns: ColumnRequest[]): EngineCommandResult {
      return runEngineCommand(['create_table', dbPath, name, buildColumnSpec(columns)]);
    },
    listTables(): EngineCommandResult {
      return runEngineCommand(['list_tables', dbPath]);
    },
    describeTable(name: string): EngineCommandResult {
      return runEngineCommand(['describe_table', dbPath, name]);
    },
    selectRows(name: string, filters?: FilterRequest[]): EngineCommandResult {
      const args = ['select', dbPath, name];
      if (filters && filters.length > 0) {
        args.push(buildFilterSpec(filters));
      }
      return runEngineCommand(args);
    },
    insertRow(name: string, values: Record<string, string | number | null>): EngineCommandResult {
      return runEngineCommand(['insert', dbPath, name, buildRowSpec(values)]);
    }
  };
}