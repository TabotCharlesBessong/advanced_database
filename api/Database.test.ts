import request from 'supertest';

import { createApp } from './app';
import { EngineClient } from './engineClient';

function createMockEngineClient(): jest.Mocked<EngineClient> {
  return {
    init: jest.fn(),
    executeSql: jest.fn(),
    createTable: jest.fn(),
    listTables: jest.fn(),
    describeTable: jest.fn(),
    selectRows: jest.fn(),
    insertRow: jest.fn()
  };
}

describe('Phase 5 Week 25-26 API endpoints', () => {
  test('GET /health returns service status', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).get('/health');

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, service: 'advanced-database-api' });
  });

  test('POST /init proxies to the engine client', async () => {
    const engineClient = createMockEngineClient();
    engineClient.init.mockReturnValue({ status: 200, body: { ok: true } });
    const app = createApp({ engineClient });

    const response = await request(app).post('/init');

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true });
    expect(engineClient.init).toHaveBeenCalledTimes(1);
  });

  test('POST /sql validates request body', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).post('/sql').send({ sql: '' });

    expect(response.status).toBe(400);
    expect(response.body).toEqual({
      ok: false,
      error: 'Request body must include a non-empty sql string'
    });
  });

  test('POST /sql forwards valid SQL to the engine client', async () => {
    const engineClient = createMockEngineClient();
    engineClient.executeSql.mockReturnValue({ status: 200, body: { ok: true, statement: 'select' } });
    const app = createApp({ engineClient });

    const response = await request(app).post('/sql').send({ sql: 'SELECT * FROM users' });

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, statement: 'select' });
    expect(engineClient.executeSql).toHaveBeenCalledWith('SELECT * FROM users');
  });

  test('POST /tables validates table payloads', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).post('/tables').send({ name: 'users', columns: [] });

    expect(response.status).toBe(400);
    expect(response.body.ok).toBe(false);
  });

  test('POST /tables creates a table via the engine client', async () => {
    const engineClient = createMockEngineClient();
    engineClient.createTable.mockReturnValue({ status: 200, body: { ok: true, table: 'users' } });
    const app = createApp({ engineClient });

    const payload = {
      name: 'users',
      columns: [
        { name: 'id', type: 'int' },
        { name: 'name', type: 'varchar', length: 100 }
      ]
    };

    const response = await request(app).post('/tables').send(payload);

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, table: 'users' });
    expect(engineClient.createTable).toHaveBeenCalledWith('users', payload.columns);
  });

  test('GET /tables returns the table catalog', async () => {
    const engineClient = createMockEngineClient();
    engineClient.listTables.mockReturnValue({ status: 200, body: { ok: true, tables: ['users'] } });
    const app = createApp({ engineClient });

    const response = await request(app).get('/tables');

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, tables: ['users'] });
    expect(engineClient.listTables).toHaveBeenCalledTimes(1);
  });

  test('GET /tables/:tableName returns table metadata', async () => {
    const engineClient = createMockEngineClient();
    engineClient.describeTable.mockReturnValue({ status: 200, body: { ok: true, table: 'users' } });
    const app = createApp({ engineClient });

    const response = await request(app).get('/tables/users');

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, table: 'users' });
    expect(engineClient.describeTable).toHaveBeenCalledWith('users');
  });

  test('GET /tables/:tableName/rows validates filter query parameters', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).get('/tables/users/rows?column=id');

    expect(response.status).toBe(400);
    expect(response.body).toEqual({
      ok: false,
      error: 'Query parameters column and value are required together'
    });
  });

  test('GET /tables/:tableName/rows forwards filters to the engine client', async () => {
    const engineClient = createMockEngineClient();
    engineClient.selectRows.mockReturnValue({ status: 200, body: { ok: true, rows: [{ id: 1 }] } });
    const app = createApp({ engineClient });

    const response = await request(app).get('/tables/users/rows?column=id&op=gte&value=1');

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, rows: [{ id: 1 }] });
    expect(engineClient.selectRows).toHaveBeenCalledWith('users', [
      { column: 'id', op: 'gte', value: 1 }
    ]);
  });

  test('POST /tables/:tableName/rows validates insert payloads', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).post('/tables/users/rows').send({ values: {} });

    expect(response.status).toBe(400);
    expect(response.body).toEqual({
      ok: false,
      error: 'Request body must include a non-empty values object'
    });
  });

  test('POST /tables/:tableName/rows inserts a row via the engine client', async () => {
    const engineClient = createMockEngineClient();
    engineClient.insertRow.mockReturnValue({ status: 200, body: { ok: true, statement: 'insert' } });
    const app = createApp({ engineClient });

    const response = await request(app)
      .post('/tables/users/rows')
      .send({ values: { id: 1, name: 'Ada' } });

    expect(response.status).toBe(200);
    expect(response.body).toEqual({ ok: true, statement: 'insert' });
    expect(engineClient.insertRow).toHaveBeenCalledWith('users', { id: 1, name: 'Ada' });
  });

  test('invalid JSON returns a structured error', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app)
      .post('/sql')
      .set('Content-Type', 'application/json')
      .send('{"sql":');

    expect(response.status).toBe(400);
    expect(response.body).toEqual({ ok: false, error: 'Invalid JSON body' });
  });

  test('unknown routes return JSON 404 responses', async () => {
    const app = createApp({ engineClient: createMockEngineClient() });

    const response = await request(app).get('/missing');

    expect(response.status).toBe(404);
    expect(response.body).toEqual({ ok: false, error: 'Route not found' });
  });
});