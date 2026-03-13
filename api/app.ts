import express, { NextFunction, Request, Response } from 'express';
import swaggerUi from 'swagger-ui-express';

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

function sendValidationError(res: Response, error: string): void {
  res.status(400).json({ ok: false, error });
}

export function createApp(dependencies: AppDependencies = {}) {
  const pool = dependencies.pool ?? getPool();
  const statementRegistry =
    dependencies.statementRegistry ?? new PreparedStatementRegistry();
  const app = express();

  // Middleware
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
    const { username, password } = req.body;

    // Simple auth for development (in production, use proper password hashing + database)
    if (!username || !password) {
      res.status(400).json({ error: 'Username and password required' });
      return;
    }

    // Accept any non-empty credentials in dev mode
    // In production: hash verification, user database lookup, etc.
    const token = generateToken(username, username);
    res.status(200).json({
      token,
      expiresIn: '24h',
      user: { username },
    });
  });

  // Initialize database
  app.post('/init', authMiddleware, async (_req, res) => {
    const client = await pool.acquire();
    try {
      const result = client.init();
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
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

    const client = await pool.acquire();
    try {
      const result = client.executeSql(req.body.sql);
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
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

    const client = await pool.acquire();
    try {
      const result = client.createTable(req.body.name, req.body.columns);
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
    }
  });

  // List tables
  app.get('/tables', authMiddleware, async (_req, res) => {
    const client = await pool.acquire();
    try {
      const result = client.listTables();
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
    }
  });

  // Describe table
  app.get('/tables/:tableName', authMiddleware, async (req, res) => {
    const client = await pool.acquire();
    try {
      const result = client.describeTable(String(req.params.tableName));
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
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

    const client = await pool.acquire();
    try {
      const result = client.selectRows(
        String(req.params.tableName),
        filterValidation.filters
      );
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
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

    const client = await pool.acquire();
    try {
      const result = client.insertRow(
        String(req.params.tableName),
        req.body.values
      );
      res.status(result.status).json(result.body);
    } finally {
      pool.release(client);
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