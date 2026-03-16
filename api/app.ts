import express, { NextFunction, Request, Response } from 'express';
import swaggerUi from 'swagger-ui-express';
import { existsSync, mkdirSync, readdirSync } from 'fs';
import path from 'path';

import { createEngineClient, EngineClient } from './engineClient';
import {
  validateCreateTableRequest,
  validateFilterRequest,
  validateRowInsertRequest,
  validateSqlRequest,
} from './validation';
import { authMiddleware, generateToken } from './auth';
import { getPool, EngineClientPool } from './pool';
import {
  PreparedStatementRegistry,
  executePreparedStatement,
} from './prepared-statements';
import { getSwaggerSpec } from './swagger-config';

interface AppDependencies {
  engineClient?: EngineClient;
  pool?: EngineClientPool;
  statementRegistry?: PreparedStatementRegistry;
}

interface AuthCredentials {
  username: string;
  password: string;
}

interface SqlNormalizationResult {
  sql: string;
  warnings: string[];
}

interface InsertExpansionResult {
  statements: string[];
  warnings: string[];
}

interface ErColumn {
  name: string;
  type: string;
  nullable: boolean;
}

interface ErTable {
  name: string;
  columns: ErColumn[];
}

interface ErRelation {
  fromTable: string;
  fromColumn: string;
  toTable: string;
  toColumn: string;
  kind: 'inferred';
}

function loadUsersFromEnv(): Map<string, string> {
  const users = new Map<string, string>();

  const rawConfig = process.env.DEMO_USERS;
  if (!rawConfig) {
    return users;
  }

  try {
    const parsed = JSON.parse(rawConfig);
    if (Array.isArray(parsed)) {
      for (const entry of parsed) {
        if (
          entry &&
          typeof entry === 'object' &&
          typeof (entry as any).username === 'string' &&
          typeof (entry as any).password === 'string'
        ) {
          users.set(
            (entry as any).username.trim(),
            (entry as any).password
          );
        }
      }
    }
  } catch {
    // Invalid DEMO_USERS configuration; leave users map empty.
  }

  return users;
}

const users = loadUsersFromEnv();

function sendValidationError(res: Response, error: string): void {
  res.status(400).json({ ok: false, error });
}

function parseAuthCredentials(body: unknown): AuthCredentials | null {
  if (!body || typeof body !== 'object') {
    return null;
  }

  const candidate = body as Record<string, unknown>;
  if (
    typeof candidate.username !== 'string' ||
    candidate.username.trim() === '' ||
    typeof candidate.password !== 'string' ||
    candidate.password.trim() === ''
  ) {
    return null;
  }

  return {
    username: candidate.username.trim(),
    password: candidate.password,
  };
}

export function createApp(dependencies: AppDependencies = {}) {
  const pool = dependencies.pool ?? getPool();
  const statementRegistry =
    dependencies.statementRegistry ?? new PreparedStatementRegistry();
  const repoRoot = resolveRepoRoot();
  const userDatabaseSelections = new Map<string, string>();
  const app = express();

  // CORS configuration
  const corsAllowedOriginsEnv = process.env.CORS_ALLOWED_ORIGINS;
  const corsAllowedOrigins = corsAllowedOriginsEnv
    ? corsAllowedOriginsEnv
        .split(',')
        .map((origin) => origin.trim())
        .filter((origin) => origin.length > 0)
    : [];
  const isDevMode = process.env.NODE_ENV === 'development';

  const getCurrentDatabase = (username?: string): string => {
    if (!username) {
      return 'advanced';
    }
    return userDatabaseSelections.get(username) ?? 'advanced';
  };

  const acquireScopedClient = async (req: Request) => {
    const currentDb = getCurrentDatabase(req.user?.username);
    if (currentDb === 'advanced') {
      const pooledClient = await pool.acquire();
      return {
        client: pooledClient,
        release: () => pool.release(pooledClient),
      };
    }

    const dbPath = getDatabasePath(repoRoot, currentDb);
    return {
      client: createEngineClient({ repoRoot, dbPath }),
      release: () => undefined,
    };
  };

  // Middleware
  app.use((req, res, next) => {
    const requestOrigin = req.headers.origin as string | undefined;

    let allowOriginHeader: string | undefined;
    if (isDevMode && corsAllowedOrigins.length === 0) {
      // Explicitly allow all origins only in development when no allowlist is configured.
      allowOriginHeader = '*';
    } else if (
      requestOrigin &&
      corsAllowedOrigins.includes(requestOrigin)
    ) {
      // Echo back the allowed origin from the configured allowlist.
      allowOriginHeader = requestOrigin;
    }

    if (allowOriginHeader) {
      res.header('Access-Control-Allow-Origin', allowOriginHeader);
      // Ensure caches differentiate responses by Origin.
      res.header('Vary', 'Origin');
      res.header(
        'Access-Control-Allow-Headers',
        'Origin, X-Requested-With, Content-Type, Accept, Authorization'
      );
      res.header(
        'Access-Control-Allow-Methods',
        'GET,POST,PUT,PATCH,DELETE,OPTIONS'
      );
    }

    if (req.method === 'OPTIONS') {
      res.sendStatus(204);
      return;
    }

    next();
  });

  app.use(express.json());

  // Swagger/OpenAPI documentation
  const swaggerSpec = getSwaggerSpec();
  app.get('/api-docs/swagger.json', (_req, res) => {
    res.status(200).json(swaggerSpec);
  });
  app.use('/api-docs', swaggerUi.serve, swaggerUi.setup(swaggerSpec));

  // Public endpoints (no auth required)
  app.get('/health', (_req, res) => {
    res.status(200).json({
      ok: true,
      service: 'advanced-database-api',
      timestamp: new Date().toISOString(),
    });
  });

  // Authentication endpoint
  app.post('/auth/login', (req, res) => {
    const credentials = parseAuthCredentials(req.body);
    if (!credentials) {
      res.status(400).json({ error: 'Username and password required' });
      return;
    }

    const storedPassword = users.get(credentials.username);
    if (!storedPassword || storedPassword !== credentials.password) {
      res.status(401).json({ error: 'Invalid username or password' });
      return;
    }

    const token = generateToken(credentials.username, credentials.username);
    res.status(200).json({
      token,
      expiresIn: '24h',
      user: { username: credentials.username },
    });
  });

  app.post('/auth/signup', (req, res) => {
    const credentials = parseAuthCredentials(req.body);
    if (!credentials) {
      res.status(400).json({ error: 'Username and password required' });
      return;
    }

    if (credentials.password.length < 6) {
      res.status(400).json({ error: 'Password must be at least 6 characters' });
      return;
    }

    if (users.has(credentials.username)) {
      res.status(409).json({ error: 'Username already exists' });
      return;
    }

    users.set(credentials.username, credentials.password);

    const token = generateToken(credentials.username, credentials.username);
    res.status(201).json({
      token,
      expiresIn: '24h',
      user: { username: credentials.username },
      message: 'Account created successfully',
    });
  });

  // Initialize database
  app.post('/init', authMiddleware, async (req, res) => {
    const { client, release } = await acquireScopedClient(req);
    try {
      const result = client.init();
      const body =
        typeof result.body === 'object' && result.body !== null
          ? (result.body as Record<string, unknown>)
          : { ok: result.status === 200 };

      res.status(result.status).json({
        ...body,
        database: getCurrentDatabase(req.user?.username),
      });
    } finally {
      release();
    }
  });

  // Execute raw SQL
  app.post('/sql', authMiddleware, async (req, res) => {
    if (!validateSqlRequest(req.body)) {
      sendValidationError(
        res,
        'Request body must include a non-empty sql string'
      );
      return;
    }

    const createDatabase = parseCreateDatabaseCommand(req.body.sql);
    if (createDatabase) {
      const dbPath = getDatabasePath(repoRoot, createDatabase);

      if (existsSync(dbPath)) {
        res
          .status(409)
          .json({
            ok: false,
            error: `Database already exists: ${createDatabase}`,
          });
        return;
      }

      const dbClient = createEngineClient({ repoRoot, dbPath });
      const initResult = dbClient.init();

      if (initResult.status !== 200) {
        res.status(initResult.status).json(initResult.body);
        return;
      }

      if (req.user?.username) {
        userDatabaseSelections.set(req.user.username, createDatabase);
      }

      res.status(200).json({
        ok: true,
        statement: 'create_database',
        database: createDatabase,
        message: 'Database created and selected',
      });
      return;
    }

    const useDatabase = parseUseDatabaseCommand(req.body.sql);
    if (useDatabase) {
      const dbPath = getDatabasePath(repoRoot, useDatabase);
      if (!existsSync(dbPath)) {
        res.status(404).json({ ok: false, error: `Database not found: ${useDatabase}` });
        return;
      }

      if (req.user?.username) {
        userDatabaseSelections.set(req.user.username, useDatabase);
      }

      res.status(200).json({
        ok: true,
        statement: 'use_database',
        database: useDatabase,
        message: 'Database selected',
      });
      return;
    }

    const normalized = normalizeSqlForEngine(req.body.sql);
    const { client, release } = await acquireScopedClient(req);
    try {
      const expandedInsert = expandInsertSqlForEngine(normalized.sql, client);
      if (expandedInsert) {
        for (let i = 0; i < expandedInsert.statements.length; i += 1) {
          const statement = expandedInsert.statements[i];
          const step = client.executeSql(statement);
          if (step.status !== 200) {
            const stepBody =
              typeof step.body === 'object' && step.body !== null
                ? (step.body as Record<string, unknown>)
                : { ok: false, error: `Insert step ${i + 1} failed` };

            res.status(step.status).json({
              ...stepBody,
              database: getCurrentDatabase(req.user?.username),
              warnings: [...normalized.warnings, ...expandedInsert.warnings],
              failedStatement: statement,
              failedAt: i + 1,
            });
            return;
          }
        }

        res.status(200).json({
          ok: true,
          statement: 'insert',
          insertedRows: expandedInsert.statements.length,
          database: getCurrentDatabase(req.user?.username),
          warnings: [...normalized.warnings, ...expandedInsert.warnings],
        });
        return;
      }

      const result = client.executeSql(normalized.sql);
      const body =
        typeof result.body === 'object' && result.body !== null
          ? (result.body as Record<string, unknown>)
          : { ok: result.status === 200 };

      res.status(result.status).json({
        ...body,
        database: getCurrentDatabase(req.user?.username),
        warnings: normalized.warnings,
      });
    } finally {
      release();
    }
  });

  app.get('/databases', authMiddleware, (req, res) => {
    const dbDir = path.resolve(repoRoot, 'data', 'databases');
    const named = existsSync(dbDir)
      ? readdirSync(dbDir)
          .filter((fileName) => fileName.endsWith('.db'))
          .map((fileName) => fileName.replace(/\.db$/i, ''))
      : [];

    const databases = Array.from(new Set(['advanced', ...named]));
    res.status(200).json({
      ok: true,
      databases,
      current: getCurrentDatabase(req.user?.username),
    });
  });

  app.get('/schema/er', authMiddleware, async (req, res) => {
    const { client, release } = await acquireScopedClient(req);
    try {
      const listResult = client.listTables();
      if (listResult.status !== 200 || !listResult.body || typeof listResult.body !== 'object') {
        res.status(listResult.status).json(listResult.body);
        return;
      }

      const listBody = listResult.body as { tables?: unknown };
      const tableNames = Array.isArray(listBody.tables)
        ? listBody.tables.filter((name): name is string => typeof name === 'string')
        : [];

      const tables: ErTable[] = [];
      for (const tableName of tableNames) {
        const describeResult = client.describeTable(tableName);
        if (
          describeResult.status !== 200 ||
          !describeResult.body ||
          typeof describeResult.body !== 'object'
        ) {
          continue;
        }

        const describeBody = describeResult.body as {
          table?: { name?: string; columns?: unknown };
        };

        if (
          !describeBody.table ||
          typeof describeBody.table.name !== 'string' ||
          !Array.isArray(describeBody.table.columns)
        ) {
          continue;
        }

        const columns: ErColumn[] = describeBody.table.columns
          .filter((col): col is ErColumn => {
            return (
              typeof col === 'object' &&
              col !== null &&
              typeof (col as ErColumn).name === 'string' &&
              typeof (col as ErColumn).type === 'string' &&
              typeof (col as ErColumn).nullable === 'boolean'
            );
          })
          .map((col) => ({
            name: col.name,
            type: col.type,
            nullable: col.nullable,
          }));

        tables.push({
          name: describeBody.table.name,
          columns,
        });
      }

      res.status(200).json({
        ok: true,
        database: getCurrentDatabase(req.user?.username),
        tables,
        relations: inferRelations(tables),
      });
    } finally {
      release();
    }
  });

  // Create table
  app.post('/tables', authMiddleware, async (req, res) => {
    if (!validateCreateTableRequest(req.body)) {
      sendValidationError(
        res,
        'Request body must include name and at least one valid column definition'
      );
      return;
    }

    const { client, release } = await acquireScopedClient(req);
    try {
      const result = client.createTable(req.body.name, req.body.columns);
      res.status(result.status).json(result.body);
    } finally {
      release();
    }
  });

  // List tables
  app.get('/tables', authMiddleware, async (_req, res) => {
    const { client, release } = await acquireScopedClient(_req);
    try {
      const result = client.listTables();
      res.status(result.status).json(result.body);
    } finally {
      release();
    }
  });

  // Describe table
  app.get('/tables/:tableName', authMiddleware, async (req, res) => {
    const { client, release } = await acquireScopedClient(req);
    try {
      const result = client.describeTable(String(req.params.tableName));
      res.status(result.status).json(result.body);
    } finally {
      release();
    }
  });

  // Select rows with optional filtering
  app.get('/tables/:tableName/rows', authMiddleware, async (req, res) => {
    const filterValidation = validateFilterRequest(
      req.query as Record<string, unknown>
    );
    if (!filterValidation.ok) {
      sendValidationError(res, filterValidation.error);
      return;
    }

    const { client, release } = await acquireScopedClient(req);
    try {
      const result = client.selectRows(
        String(req.params.tableName),
        filterValidation.filters
      );
      res.status(result.status).json(result.body);
    } finally {
      release();
    }
  });

  // Insert row
  app.post('/tables/:tableName/rows', authMiddleware, async (req, res) => {
    if (!validateRowInsertRequest(req.body)) {
      sendValidationError(
        res,
        'Request body must include a non-empty values object'
      );
      return;
    }

    const { client, release } = await acquireScopedClient(req);
    try {
      const result = client.insertRow(
        String(req.params.tableName),
        req.body.values
      );
      res.status(result.status).json(result.body);
    } finally {
      release();
    }
  });

  // Prepared statements - Prepare
  app.post('/prepare', authMiddleware, (req, res) => {
    const { sql } = req.body;

    if (!sql || typeof sql !== 'string' || sql.trim() === '') {
      sendValidationError(res, 'Request body must include a non-empty sql string');
      return;
    }

    try {
      const stmt = statementRegistry.prepare(sql);
      res.status(200).json(stmt);
    } catch (error) {
      sendValidationError(
        res,
        error instanceof Error ? error.message : 'Failed to prepare statement'
      );
    }
  });

  // Prepared statements - Execute
  app.post('/execute', authMiddleware, async (req, res) => {
    const { statementId, parameters } = req.body;

    if (!statementId || !Array.isArray(parameters)) {
      sendValidationError(
        res,
        'Request body must include statementId and parameters array'
      );
      return;
    }

    const client = await pool.acquire();
    try {
      const result = await executePreparedStatement(
        statementRegistry,
        client,
        statementId,
        parameters
      );
      res.status(200).json(result);
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Execution failed';
      if (errorMsg.includes('not found')) {
        res.status(404).json({ error: errorMsg });
      } else {
        res.status(400).json({ error: errorMsg });
      }
    } finally {
      pool.release(client);
    }
  });

  // Connection pool statistics
  app.get('/stats/pool', authMiddleware, (_req, res) => {
    const stats = pool.stats();
    res.status(200).json(stats);
  });

  // 404 handler
  app.use((_req, res) => {
    res.status(404).json({ ok: false, error: 'Route not found' });
  });

  // Error handler
  app.use((error: Error, _req: Request, res: Response, _next: NextFunction) => {
    if (error instanceof SyntaxError && 'body' in error) {
      res.status(400).json({ ok: false, error: 'Invalid JSON body' });
      return;
    }

    res.status(500).json({ ok: false, error: 'Internal server error' });
  });

  return app;
}

function resolveRepoRoot(): string {
  return existsSync(path.resolve(process.cwd(), 'build'))
    ? process.cwd()
    : path.resolve(process.cwd(), '..');
}

function normalizeDatabaseName(input: string): string {
  return input.trim().replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
}

function getDatabasePath(repoRoot: string, databaseName: string): string {
  if (databaseName === 'advanced') {
    return path.resolve(repoRoot, 'data', 'advanced.db');
  }

  const dbDir = path.resolve(repoRoot, 'data', 'databases');
  mkdirSync(dbDir, { recursive: true });
  return path.resolve(dbDir, `${databaseName}.db`);
}

function stripSqlLineComments(sql: string): string {
  return sql
    .split(/\r?\n/)
    .map((line) => {
      const idx = line.indexOf('--');
      return idx >= 0 ? line.slice(0, idx) : line;
    })
    .join('\n');
}

function parseCreateDatabaseCommand(sql: string): string | null {
  const match = sql.match(
    /^\s*CREATE\s+DATABASE\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;?\s*$/i
  );
  if (!match) {
    return null;
  }

  return normalizeDatabaseName(match[1]);
}

function parseUseDatabaseCommand(sql: string): string | null {
  const match = sql.match(/^\s*USE\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;?\s*$/i);
  if (!match) {
    return null;
  }

  return normalizeDatabaseName(match[1]);
}

function inferRelations(tables: ErTable[]): ErRelation[] {
  const byName = new Map<string, ErTable>();
  for (const table of tables) {
    byName.set(table.name.toLowerCase(), table);
  }

  const relations: ErRelation[] = [];

  for (const table of tables) {
    for (const column of table.columns) {
      const lower = column.name.toLowerCase();
      if (!lower.endsWith('id') || lower === 'id') {
        continue;
      }

      const base = lower.replace(/_?id$/, '');
      const candidates = [base, `${base}s`];

      for (const candidate of candidates) {
        const target = byName.get(candidate);
        if (!target) {
          continue;
        }

        const idColumn = target.columns.find((col) => col.name.toLowerCase() === 'id');
        if (!idColumn) {
          continue;
        }

        relations.push({
          fromTable: table.name,
          fromColumn: column.name,
          toTable: target.name,
          toColumn: idColumn.name,
          kind: 'inferred',
        });
        break;
      }
    }
  }

  return relations;
}

function normalizeSqlForEngine(rawSql: string): SqlNormalizationResult {
  const warnings: string[] = [];
  let sql = stripSqlLineComments(rawSql).trim();

  if (/^\s*CREATE\s+TABLE\b/i.test(sql)) {
    const before = sql;
    sql = sql
      .replace(/\bPRIMARY\s+KEY\b/gi, '')
      .replace(/\bUNIQUE\b/gi, '')
      .replace(/\bDATE\b/gi, 'TEXT')
      .replace(/,\s*CONSTRAINT\s+[a-zA-Z_][a-zA-Z0-9_]*\s+CHECK\s*\([^)]*\)/gi, '')
      .replace(/\bCHECK\s*\([^)]*\)/gi, '')
      .replace(/\bCURRENT_DATE\b/gi, "'CURRENT_DATE'")
      .replace(/\s+,/g, ',')
      .replace(/,\s*\)/g, ')')
      .replace(/\s{2,}/g, ' ')
      .trim();

    if (sql !== before) {
      warnings.push(
        'Normalized CREATE TABLE for engine compatibility (removed PRIMARY KEY/UNIQUE and mapped DATE to TEXT).'
      );
    }
  }

  return { sql, warnings };
}

function splitTupleValues(tuple: string): string[] {
  const values: string[] = [];
  let token = '';
  let inString = false;

  for (let i = 0; i < tuple.length; i += 1) {
    const ch = tuple[i];

    if (ch === "'" && tuple[i - 1] !== '\\') {
      inString = !inString;
      token += ch;
      continue;
    }

    if (ch === ',' && !inString) {
      values.push(token.trim());
      token = '';
      continue;
    }

    token += ch;
  }

  if (token.trim().length > 0) {
    values.push(token.trim());
  }

  return values;
}

function splitValueTuples(valuesSegment: string): string[] {
  const tuples: string[] = [];
  let depth = 0;
  let inString = false;
  let start = -1;

  for (let i = 0; i < valuesSegment.length; i += 1) {
    const ch = valuesSegment[i];

    if (ch === "'" && valuesSegment[i - 1] !== '\\') {
      inString = !inString;
      continue;
    }

    if (inString) {
      continue;
    }

    if (ch === '(') {
      if (depth === 0) {
        start = i + 1;
      }
      depth += 1;
      continue;
    }

    if (ch === ')') {
      depth -= 1;
      if (depth === 0 && start >= 0) {
        tuples.push(valuesSegment.slice(start, i));
        start = -1;
      }
    }
  }

  return tuples;
}

function parseSchemaColumns(body: unknown): string[] | null {
  if (!body || typeof body !== 'object') {
    return null;
  }

  const maybeTable = (body as { table?: unknown }).table;
  if (!maybeTable || typeof maybeTable !== 'object') {
    return null;
  }

  const maybeColumns = (maybeTable as { columns?: unknown }).columns;
  if (!Array.isArray(maybeColumns)) {
    return null;
  }

  const names = maybeColumns
    .map((col) => (typeof col === 'object' && col ? (col as { name?: unknown }).name : undefined))
    .filter((name): name is string => typeof name === 'string');

  return names.length > 0 ? names : null;
}

function normalizeWhitespaceOutsideStrings(sql: string): string {
  let result = '';
  let inQuote: string | null = null;
  let lastWasWhitespace = false;
  let i = 0;

  while (i < sql.length) {
    const ch = sql[i];

    if (inQuote) {
      result += ch;

      if (ch === inQuote) {
        // Handle doubled single-quote inside string literals: ''
        if (ch === "'" && i + 1 < sql.length && sql[i + 1] === "'") {
          result += sql[i + 1];
          i += 2;
          continue;
        }
        inQuote = null;
        i += 1;
        continue;
      }

      // Handle backslash escapes inside quoted strings (\')
      if (ch === '\\' && i + 1 < sql.length) {
        result += sql[i + 1];
        i += 2;
        continue;
      }

      i += 1;
      continue;
    } else {
      // Enter quoted region
      if (ch === "'" || ch === '"' || ch === '`') {
        inQuote = ch;
        result += ch;
        lastWasWhitespace = false;
        i += 1;
        continue;
      }

      // Collapse whitespace when outside quotes
      if (/\s/.test(ch)) {
        if (!lastWasWhitespace && result.length > 0) {
          result += ' ';
          lastWasWhitespace = true;
        }
        i += 1;
        continue;
      }

      result += ch;
      lastWasWhitespace = false;
      i += 1;
    }
  }

  // Trim leading/trailing whitespace that is necessarily outside any quoted string
  return result.trim();
}

function expandInsertSqlForEngine(sql: string, client: EngineClient): InsertExpansionResult | null {
  const compact = normalizeWhitespaceOutsideStrings(sql);
  const insertWithColumns = compact.match(
    /^INSERT\s+INTO\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]+)\)\s+VALUES\s+([\s\S]+?);?$/i
  );

  if (insertWithColumns) {
    const tableName = insertWithColumns[1];
    const insertColumns = insertWithColumns[2]
      .split(',')
      .map((column) => column.trim())
      .filter((column) => column.length > 0);
    const tuples = splitValueTuples(insertWithColumns[3]);

    if (tuples.length === 0) {
      return null;
    }

    const describeResult = client.describeTable(tableName);
    const schemaColumns = parseSchemaColumns(describeResult.body);
    if (describeResult.status !== 200 || !schemaColumns) {
      return null;
    }

    const columnIndex = new Map<string, number>();
    insertColumns.forEach((column, index) => {
      columnIndex.set(column.toLowerCase(), index);
    });

    const statements = tuples.map((tuple) => {
      const rowValues = splitTupleValues(tuple);
      const fullValues = schemaColumns.map((schemaColumn) => {
        const index = columnIndex.get(schemaColumn.toLowerCase());
        if (index === undefined) {
          return 'NULL';
        }
        return rowValues[index] ?? 'NULL';
      });

      return `INSERT INTO ${tableName} VALUES (${fullValues.join(', ')});`;
    });

    return {
      statements,
      warnings: [
        'Expanded INSERT column-list syntax to engine-compatible INSERT VALUES statements.',
      ],
    };
  }

  const insertMultiValues = compact.match(
    /^INSERT\s+INTO\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+VALUES\s+([\s\S]+?);?$/i
  );
  if (!insertMultiValues) {
    return null;
  }

  const tableName = insertMultiValues[1];
  const tuples = splitValueTuples(insertMultiValues[2]);
  if (tuples.length <= 1) {
    return null;
  }

  return {
    statements: tuples.map((tuple) => `INSERT INTO ${tableName} VALUES (${tuple});`),
    warnings: ['Expanded multi-row INSERT into sequential INSERT statements.'],
  };
}