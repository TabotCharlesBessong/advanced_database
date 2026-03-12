import express, { NextFunction, Request, Response } from 'express';

import { createEngineClient, EngineClient } from './engineClient';
import {
  validateCreateTableRequest,
  validateFilterRequest,
  validateRowInsertRequest,
  validateSqlRequest
} from './validation';

interface AppDependencies {
  engineClient?: EngineClient;
}

function sendValidationError(res: Response, error: string): void {
  res.status(400).json({ ok: false, error });
}

export function createApp(dependencies: AppDependencies = {}) {
  const engineClient = dependencies.engineClient ?? createEngineClient();
  const app = express();

  app.use(express.json());

  app.get('/health', (_req, res) => {
    res.status(200).json({ ok: true, service: 'advanced-database-api' });
  });

  app.post('/init', (_req, res) => {
    const result = engineClient.init();
    res.status(result.status).json(result.body);
  });

  app.post('/sql', (req, res) => {
    if (!validateSqlRequest(req.body)) {
      sendValidationError(res, 'Request body must include a non-empty sql string');
      return;
    }

    const result = engineClient.executeSql(req.body.sql);
    res.status(result.status).json(result.body);
  });

  app.post('/tables', (req, res) => {
    if (!validateCreateTableRequest(req.body)) {
      sendValidationError(res, 'Request body must include name and at least one valid column definition');
      return;
    }

    const result = engineClient.createTable(req.body.name, req.body.columns);
    res.status(result.status).json(result.body);
  });

  app.get('/tables', (_req, res) => {
    const result = engineClient.listTables();
    res.status(result.status).json(result.body);
  });

  app.get('/tables/:tableName', (req, res) => {
    const result = engineClient.describeTable(req.params.tableName);
    res.status(result.status).json(result.body);
  });

  app.get('/tables/:tableName/rows', (req, res) => {
    const filterValidation = validateFilterRequest(req.query as Record<string, unknown>);
    if (!filterValidation.ok) {
      sendValidationError(res, filterValidation.error);
      return;
    }

    const result = engineClient.selectRows(req.params.tableName, filterValidation.filters);
    res.status(result.status).json(result.body);
  });

  app.post('/tables/:tableName/rows', (req, res) => {
    if (!validateRowInsertRequest(req.body)) {
      sendValidationError(res, 'Request body must include a non-empty values object');
      return;
    }

    const result = engineClient.insertRow(req.params.tableName, req.body.values);
    res.status(result.status).json(result.body);
  });

  app.use((_req, res) => {
    res.status(404).json({ ok: false, error: 'Route not found' });
  });

  app.use((error: Error, _req: Request, res: Response, _next: NextFunction) => {
    if (error instanceof SyntaxError && 'body' in error) {
      res.status(400).json({ ok: false, error: 'Invalid JSON body' });
      return;
    }

    res.status(500).json({ ok: false, error: 'Internal server error' });
  });

  return app;
}