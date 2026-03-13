import { FilterRequest } from './engineClient';

export interface CreateTableRequest {
  name: string;
  columns: Array<{
    name: string;
    type: string;
    nullable?: boolean;
    length?: number;
  }>;
}

export interface RowInsertRequest {
  values: Record<string, string | number | null>;
}

export interface SqlRequest {
  sql: string;
}

const allowedTypes = new Set(['int', 'text', 'varchar']);
const allowedOps = new Set<NonNullable<FilterRequest['op']>>(['eq', 'neq', 'lt', 'lte', 'gt', 'gte']);

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function validateSqlRequest(body: unknown): body is SqlRequest {
  return isPlainObject(body) && typeof body.sql === 'string' && body.sql.trim().length > 0;
}

export function validateCreateTableRequest(body: unknown): body is CreateTableRequest {
  if (!isPlainObject(body) || typeof body.name !== 'string' || body.name.trim().length === 0) {
    return false;
  }

  if (!Array.isArray(body.columns) || body.columns.length === 0) {
    return false;
  }

  return body.columns.every((column) => {
    if (!isPlainObject(column) || typeof column.name !== 'string' || column.name.trim().length === 0) {
      return false;
    }

    if (typeof column.type !== 'string' || !allowedTypes.has(column.type.toLowerCase())) {
      return false;
    }

    if (column.nullable !== undefined && typeof column.nullable !== 'boolean') {
      return false;
    }

    if (column.length !== undefined && (!Number.isInteger(column.length) || Number(column.length) <= 0)) {
      return false;
    }

    return true;
  });
}

export function validateRowInsertRequest(body: unknown): body is RowInsertRequest {
  return isPlainObject(body) && isPlainObject(body.values) && Object.keys(body.values).length > 0;
}

export function validateFilterRequest(query: Record<string, unknown>): { ok: true; filters?: FilterRequest[] } | { ok: false; error: string } {
  const column = query.column;
  const value = query.value;
  const op = query.op;

  if (column === undefined && value === undefined && op === undefined) {
    return { ok: true };
  }

  if (typeof column !== 'string' || column.trim().length === 0 || value === undefined) {
    return { ok: false, error: 'Query parameters column and value are required together' };
  }

  if (op !== undefined && (typeof op !== 'string' || !allowedOps.has(op as NonNullable<FilterRequest['op']>))) {
    return { ok: false, error: 'Query parameter op must be one of eq, neq, lt, lte, gt, gte' };
  }

  const normalizedValue = typeof value === 'string' && /^-?\d+$/.test(value)
    ? Number(value)
    : String(value);

  return {
    ok: true,
    filters: [{ column, value: normalizedValue, op: (op as FilterRequest['op'] | undefined) ?? 'eq' }]
  };
}