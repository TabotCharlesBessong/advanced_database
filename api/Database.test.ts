import request from 'supertest';

import { createApp } from './app';
import { EngineClient } from './engineClient';
import { EngineClientPool } from './pool';
import { PreparedStatementRegistry } from './prepared-statements';

function createMockEngineClient(): jest.Mocked<EngineClient> {
  return {
    init: jest.fn(),
    executeSql: jest.fn(),
    createTable: jest.fn(),
    listTables: jest.fn(),
    describeTable: jest.fn(),
    selectRows: jest.fn(),
    insertRow: jest.fn(),
  };
}

function createMockPool(): jest.Mocked<EngineClientPool> {
  const mockClient = createMockEngineClient();
  return {
    acquire: jest.fn().mockResolvedValue(mockClient),
    release: jest.fn(),
    stats: jest.fn().mockReturnValue({
      totalConnections: 2,
      inUse: 1,
      idle: 1,
      waitingRequests: 0,
      poolSize: { min: 2, max: 10 },
    }),
    drain: jest.fn().mockResolvedValue(undefined),
  } as unknown as jest.Mocked<EngineClientPool>;
}

describe('Phase 5 Week 27-28 API endpoints', () => {
  describe('Authentication (JWT)', () => {
    test('GET /health is public (no auth required)', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).get('/health');

      expect(response.status).toBe(200);
      expect(response.body.ok).toBe(true);
      expect(response.body.service).toBe('advanced-database-api');
    });

    test('POST /auth/login returns JWT token for valid credentials', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      expect(response.status).toBe(200);
      expect(response.body).toHaveProperty('token');
      expect(response.body).toHaveProperty('expiresIn');
      expect(response.body.user.username).toBe('testuser');
    });

    test('POST /auth/login rejects missing credentials', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).post('/auth/login').send({
        username: 'testuser',
      });

      expect(response.status).toBe(400);
      expect(response.body.error).toBe('Username and password required');
    });

    test('Protected endpoint rejects request without auth header', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).post('/init');

      expect(response.status).toBe(401);
      expect(response.body.error).toBe('Missing authorization header');
    });

    test('Protected endpoint rejects malformed auth header', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app)
        .post('/init')
        .set('Authorization', 'InvalidFormat');

      expect(response.status).toBe(401);
      expect(response.body.error).toBe('Invalid authorization header format');
    });

    test('Protected endpoint accepts valid JWT token', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.init.mockReturnValue({ status: 200, body: { ok: true } });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // First, get a valid token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      // Use token to call protected endpoint
      const response = await request(app)
        .post('/init')
        .set('Authorization', `Bearer ${token}`);

      expect(response.status).toBe(200);
      expect(mockClient.init).toHaveBeenCalled();
    });
  });

  describe('Connection Pool', () => {
    test('GET /stats/pool returns pool statistics', async () => {
      const pool = createMockPool();
      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get a valid token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .get('/stats/pool')
        .set('Authorization', `Bearer ${token}`);

      expect(response.status).toBe(200);
      expect(response.body).toHaveProperty('totalConnections');
      expect(response.body).toHaveProperty('inUse');
      expect(response.body).toHaveProperty('idle');
      expect(response.body).toHaveProperty('waitingRequests');
      expect(response.body).toHaveProperty('poolSize');
      expect(pool.stats).toHaveBeenCalled();
    });

    test('Database endpoints acquire and release connections', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.executeSql.mockReturnValue({
        status: 200,
        body: { ok: true, statement: 'select' },
      });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      // Call SQL endpoint
      await request(app)
        .post('/sql')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: 'SELECT * FROM users' });

      expect(pool.acquire).toHaveBeenCalled();
      expect(pool.release).toHaveBeenCalledWith(mockClient);
    });
  });

  describe('Prepared Statements', () => {
    test('POST /prepare creates a prepared statement', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/prepare')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: 'SELECT * FROM users WHERE id = ?' });

      expect(response.status).toBe(200);
      expect(response.body).toHaveProperty('id');
      expect(response.body.sql).toBe('SELECT * FROM users WHERE id = ?');
      expect(response.body.paramCount).toBe(1);
    });

    test('POST /prepare validates SQL parameter', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/prepare')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: '' });

      expect(response.status).toBe(400);
      expect(response.body.error).toContain('non-empty sql string');
    });

    test('POST /execute runs a prepared statement with parameters', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.executeSql.mockReturnValue({
        status: 200,
        body: { ok: true, rows: [{ id: 1, name: 'Ada' }] },
      });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      // Prepare statement
      const prepareResponse = await request(app)
        .post('/prepare')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: 'SELECT * FROM users WHERE id = ?' });

      const statementId = prepareResponse.body.id;

      // Execute with parameters
      const executeResponse = await request(app)
        .post('/execute')
        .set('Authorization', `Bearer ${token}`)
        .send({
          statementId,
          parameters: [1],
        });

      expect(executeResponse.status).toBe(200);
      expect(mockClient.executeSql).toHaveBeenCalledWith(
        'SELECT * FROM users WHERE id = 1'
      );
    });

    test('POST /execute rejects execution of non-existent statement', async () => {
      const pool = createMockPool();
      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/execute')
        .set('Authorization', `Bearer ${token}`)
        .send({
          statementId: 'non-existent-id',
          parameters: [1],
        });

      expect(response.status).toBe(404);
      expect(response.body.error).toContain('not found');
    });

    test('POST /execute validates parameter count', async () => {
      const pool = createMockPool();
      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      // Prepare statement
      const prepareResponse = await request(app)
        .post('/prepare')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: 'SELECT * FROM users WHERE id = ? AND name = ?' });

      const statementId = prepareResponse.body.id;

      // Execute with wrong parameter count
      const executeResponse = await request(app)
        .post('/execute')
        .set('Authorization', `Bearer ${token}`)
        .send({
          statementId,
          parameters: [1], // Expected 2 parameters
        });

      expect(executeResponse.status).toBe(400);
      expect(executeResponse.body.error).toContain('Expected 2 parameters');
    });
  });

  describe('Swagger/OpenAPI Documentation', () => {
    test('GET /api-docs serves Swagger UI HTML', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).get('/api-docs/');

      expect(response.status).toBe(200);
      expect(response.type).toBe('text/html');
      expect(response.text).toContain('swagger-ui');
    });

    test('GET /api-docs/swagger.json serves OpenAPI spec', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).get('/api-docs/swagger.json');

      expect(response.status).toBe(200);
      expect(response.body.openapi).toBe('3.0.0');
      expect(response.body.info.title).toBe('Advanced Database API');
      expect(response.body.info.version).toBe('0.4.0');
      expect(response.body).toHaveProperty('paths');
      expect(response.body.paths).toHaveProperty('/health');
      expect(response.body.paths).toHaveProperty('/auth/login');
      expect(response.body.paths).toHaveProperty('/tables');
      expect(response.body.paths).toHaveProperty('/prepare');
      expect(response.body.paths).toHaveProperty('/execute');
    });
  });

  describe('Week 25-26 Endpoints (Updated for Auth)', () => {
    test('POST /init requires authentication', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).post('/init');

      expect(response.status).toBe(401);
      expect(response.body.error).toBe('Missing authorization header');
    });

    test('POST /init proxies to engine client with valid token', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.init.mockReturnValue({ status: 200, body: { ok: true } });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/init')
        .set('Authorization', `Bearer ${token}`);

      expect(response.status).toBe(200);
      expect(response.body).toEqual({ ok: true });
      expect(mockClient.init).toHaveBeenCalledTimes(1);
    });

    test('POST /sql validates request body with valid token', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/sql')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: '' });

      expect(response.status).toBe(400);
      expect(response.body).toEqual({
        ok: false,
        error: 'Request body must include a non-empty sql string',
      });
    });

    test('POST /sql forwards valid SQL to engine client', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.executeSql.mockReturnValue({
        status: 200,
        body: { ok: true, statement: 'select' },
      });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .post('/sql')
        .set('Authorization', `Bearer ${token}`)
        .send({ sql: 'SELECT * FROM users' });

      expect(response.status).toBe(200);
      expect(response.body).toEqual({ ok: true, statement: 'select' });
      expect(mockClient.executeSql).toHaveBeenCalledWith('SELECT * FROM users');
    });

    test('GET /tables requires authentication', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).get('/tables');

      expect(response.status).toBe(401);
    });

    test('GET /tables returns catalog with valid token', async () => {
      const pool = createMockPool();
      const mockClient = createMockEngineClient();
      mockClient.listTables.mockReturnValue({
        status: 200,
        body: { ok: true, tables: ['users'] },
      });
      (pool.acquire as jest.Mock).mockResolvedValue(mockClient);

      const app = createApp({
        pool,
        statementRegistry: new PreparedStatementRegistry(),
      });

      // Get token
      const loginResponse = await request(app).post('/auth/login').send({
        username: 'testuser',
        password: 'testpass',
      });

      const token = loginResponse.body.token;

      const response = await request(app)
        .get('/tables')
        .set('Authorization', `Bearer ${token}`);

      expect(response.status).toBe(200);
      expect(response.body).toEqual({ ok: true, tables: ['users'] });
      expect(mockClient.listTables).toHaveBeenCalledTimes(1);
    });

    test('Invalid JSON returns a structured error', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app)
        .post('/auth/login')
        .set('Content-Type', 'application/json')
        .send('{"sql":');

      expect(response.status).toBe(400);
      expect(response.body).toEqual({ ok: false, error: 'Invalid JSON body' });
    });

    test('Unknown routes return JSON 404 response', async () => {
      const app = createApp({
        pool: createMockPool(),
        statementRegistry: new PreparedStatementRegistry(),
      });

      const response = await request(app).get('/missing-route');

      expect(response.status).toBe(404);
      expect(response.body).toEqual({ ok: false, error: 'Route not found' });
    });
  });
});
