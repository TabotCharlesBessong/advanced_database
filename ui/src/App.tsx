import { useMemo, useState } from 'react';
import {
  ConnectionConfig,
  describeTable,
  executeSql,
  getTableRows,
  initializeDatabase,
  insertTableRow,
  listTables,
  login,
  signup,
  TableSchema,
} from './api/client';
import { ConnectionPanel } from './components/ConnectionPanel';
import { QueryResult } from './components/QueryResult';
import { RowEditorDialog } from './components/RowEditorDialog';
import { SchemaDiagram } from './components/SchemaDiagram';
import { SqlEditor } from './components/SqlEditor';
import { TableExplorer } from './components/TableExplorer';
import { ResultGrid } from './components/ResultGrid';

type ActiveView = 'query' | 'browser' | 'docs';

interface SessionState {
  baseUrl: string;
  token: string;
  username: string;
}

const DEFAULT_API_URL = 'http://localhost:3000';

export function App() {
  const [activeView, setActiveView] = useState<ActiveView>('query');
  const [busy, setBusy] = useState(false);
  const [browserBusy, setBrowserBusy] = useState(false);
  const [session, setSession] = useState<SessionState | null>(null);
  const [queryResult, setQueryResult] = useState<unknown>(null);
  const [statusMessage, setStatusMessage] = useState('Not connected.');
  const [tables, setTables] = useState<string[]>([]);
  const [selectedTable, setSelectedTable] = useState<string | null>(null);
  const [selectedSchema, setSelectedSchema] = useState<TableSchema | null>(null);
  const [selectedRows, setSelectedRows] = useState<Array<Record<string, unknown>>>([]);
  const [rowDialogOpen, setRowDialogOpen] = useState(false);

  const connected = Boolean(session);

  const docsUrl = useMemo(() => {
    if (session) {
      return `${session.baseUrl}/api-docs`;
    }
    return `${DEFAULT_API_URL}/api-docs`;
  }, [session]);

  const refreshTables = async (baseUrl: string, token: string) => {
    setBrowserBusy(true);
    const response = await listTables(baseUrl, token);
    if (!response.ok || !response.data) {
      setStatusMessage(`Failed to load tables: ${response.error}`);
      setTables([]);
      setBrowserBusy(false);
      return;
    }

    setTables(response.data.tables);
    setStatusMessage(`Loaded ${response.data.tables.length} table(s).`);

    if (response.data.tables.length === 0) {
      setSelectedTable(null);
      setSelectedSchema(null);
      setSelectedRows([]);
    } else if (!selectedTable || !response.data.tables.includes(selectedTable)) {
      const firstTable = response.data.tables[0];
      await loadTableDetails(baseUrl, token, firstTable);
    }

    setBrowserBusy(false);
  };

  const loadTableDetails = async (baseUrl: string, token: string, tableName: string) => {
    setBrowserBusy(true);
    setSelectedTable(tableName);

    const [schemaResponse, rowsResponse] = await Promise.all([
      describeTable(baseUrl, token, tableName),
      getTableRows(baseUrl, token, tableName),
    ]);

    if (!schemaResponse.ok || !schemaResponse.data) {
      setStatusMessage(`Failed to load schema for ${tableName}: ${schemaResponse.error}`);
      setBrowserBusy(false);
      return;
    }

    if (!rowsResponse.ok || !rowsResponse.data) {
      setStatusMessage(`Failed to load rows for ${tableName}: ${rowsResponse.error}`);
      setBrowserBusy(false);
      return;
    }

    setSelectedSchema(schemaResponse.data.table);
    setSelectedRows(rowsResponse.data.rows as Array<Record<string, unknown>>);
    setStatusMessage(`Loaded ${tableName} (${rowsResponse.data.rows.length} rows).`);
    setBrowserBusy(false);
  };

  const handleConnect = async (config: ConnectionConfig) => {
    setBusy(true);
    setStatusMessage('Authenticating...');

    const loginResponse = await login(config);
    if (!loginResponse.ok || !loginResponse.data) {
      setStatusMessage(`Connection failed: ${loginResponse.error}`);
      setBusy(false);
      return;
    }

    setStatusMessage('Initializing database...');
    const initResponse = await initializeDatabase(config.baseUrl, loginResponse.data.token);
    if (!initResponse.ok) {
      setStatusMessage(`Connected, but init failed: ${initResponse.error}`);
      setBusy(false);
      return;
    }

    setSession({
      baseUrl: config.baseUrl,
      token: loginResponse.data.token,
      username: loginResponse.data.user.username,
    });
    setStatusMessage(`Connected as ${loginResponse.data.user.username}`);
    await refreshTables(config.baseUrl, loginResponse.data.token);
    setBusy(false);
  };

  const handleSignup = async (config: ConnectionConfig) => {
    setBusy(true);
    setStatusMessage('Creating account...');

    const signupResponse = await signup(config);
    if (!signupResponse.ok || !signupResponse.data) {
      setStatusMessage(`Signup failed: ${signupResponse.error}`);
      setBusy(false);
      return;
    }

    setStatusMessage('Account created. Initializing database...');
    const initResponse = await initializeDatabase(config.baseUrl, signupResponse.data.token);
    if (!initResponse.ok) {
      setStatusMessage(`Signup succeeded, but init failed: ${initResponse.error}`);
      setBusy(false);
      return;
    }

    setSession({
      baseUrl: config.baseUrl,
      token: signupResponse.data.token,
      username: signupResponse.data.user.username,
    });
    setStatusMessage(`Welcome, ${signupResponse.data.user.username}. You're connected.`);
    await refreshTables(config.baseUrl, signupResponse.data.token);
    setBusy(false);
  };

  const handleDisconnect = () => {
    setSession(null);
    setQueryResult(null);
    setTables([]);
    setSelectedTable(null);
    setSelectedSchema(null);
    setSelectedRows([]);
    setRowDialogOpen(false);
    setStatusMessage('Disconnected.');
  };

  const handleRunSql = async (sql: string) => {
    if (!session) {
      setStatusMessage('Connect to the API first.');
      return;
    }

    setBusy(true);
    setStatusMessage('Running SQL...');

    const response = await executeSql(session.baseUrl, session.token, sql);
    if (!response.ok) {
      setStatusMessage(`Query failed: ${response.error}`);
      setBusy(false);
      return;
    }

    setQueryResult(response.data);
    setStatusMessage('Query completed successfully.');

    const statement =
      typeof response.data === 'object' && response.data !== null
        ? (response.data as { statement?: string }).statement
        : undefined;

    if (statement === 'create_table' || statement === 'insert') {
      await refreshTables(session.baseUrl, session.token);
    }

    setBusy(false);
  };

  const handleRefreshTables = async () => {
    if (!session) {
      return;
    }

    await refreshTables(session.baseUrl, session.token);
  };

  const handleSelectTable = async (tableName: string) => {
    if (!session) {
      return;
    }

    await loadTableDetails(session.baseUrl, session.token, tableName);
  };

  const handleInsertRow = async (values: Record<string, string | number | null>) => {
    if (!session || !selectedTable) {
      return;
    }

    setBrowserBusy(true);
    const response = await insertTableRow(
      session.baseUrl,
      session.token,
      selectedTable,
      values
    );

    if (!response.ok) {
      setStatusMessage(`Insert failed: ${response.error}`);
      setBrowserBusy(false);
      return;
    }

    setRowDialogOpen(false);
    setStatusMessage(`Inserted row into ${selectedTable}.`);
    await loadTableDetails(session.baseUrl, session.token, selectedTable);
    setBrowserBusy(false);
  };

  return (
    <div className="app-shell">
      <div className="ambient-shape ambient-one" />
      <div className="ambient-shape ambient-two" />

      <header className="topbar">
        <div>
          <p className="eyebrow">Phase 6 • Week 31-32</p>
          <h1>Advanced Database Studio</h1>
        </div>

        <nav className="tabs" aria-label="Primary views">
          <button
            className={activeView === 'query' ? 'tab active' : 'tab'}
            onClick={() => setActiveView('query')}
          >
            Query Workspace
          </button>
          <button
            className={activeView === 'browser' ? 'tab active' : 'tab'}
            onClick={() => setActiveView('browser')}
          >
            Data Browser
          </button>
          <button
            className={activeView === 'docs' ? 'tab active' : 'tab'}
            onClick={() => setActiveView('docs')}
          >
            API Docs
          </button>
        </nav>
      </header>

      <main className="layout">
        <aside className="sidebar">
          <ConnectionPanel
            busy={busy}
            isConnected={connected}
            onConnect={handleConnect}
            onSignup={handleSignup}
            onDisconnect={handleDisconnect}
            defaultUrl={DEFAULT_API_URL}
          />

          <section className="panel session-panel">
            <header className="panel-header">
              <h2>Session</h2>
            </header>
            <ul>
              <li>Status: {statusMessage}</li>
              <li>User: {session?.username ?? 'n/a'}</li>
              <li>Endpoint: {session?.baseUrl ?? DEFAULT_API_URL}</li>
            </ul>
          </section>
        </aside>

        <section className="content">
          {activeView === 'query' ? (
            <>
              <SqlEditor busy={busy} disabled={!connected} onRun={handleRunSql} />
              {queryResult !== null && <QueryResult result={queryResult} />}
            </>
          ) : activeView === 'browser' ? (
            <section className="browser-layout">
              <TableExplorer
                tables={tables}
                selectedTable={selectedTable}
                busy={browserBusy}
                disabled={!connected}
                onSelect={handleSelectTable}
                onRefresh={handleRefreshTables}
              />

              <div className="browser-main">
                <SchemaDiagram schema={selectedSchema} />

                <section className="panel rows-panel">
                  <header className="panel-header compact">
                    <h2>Rows {selectedTable ? `· ${selectedTable}` : ''}</h2>
                    <div className="button-row">
                      <button
                        type="button"
                        className="secondary"
                        onClick={() => setRowDialogOpen(true)}
                        disabled={!connected || !selectedSchema || browserBusy}
                      >
                        Insert Row
                      </button>
                    </div>
                  </header>

                  {selectedTable ? (
                    <ResultGrid rows={selectedRows} title="Table Rows" />
                  ) : (
                    <p className="empty-state">Choose a table to browse its rows.</p>
                  )}
                </section>
              </div>
            </section>
          ) : (
            <section className="panel docs-panel">
              <header className="panel-header">
                <h2>Swagger API Documentation</h2>
                <p>Opens your backend OpenAPI interface in a new tab.</p>
              </header>

              <div className="button-row">
                <a className="button-link" href={docsUrl} target="_blank" rel="noreferrer">
                  Open {docsUrl}
                </a>
              </div>
            </section>
          )}
        </section>
      </main>

      <RowEditorDialog
        open={rowDialogOpen}
        schema={selectedSchema}
        busy={browserBusy}
        onClose={() => setRowDialogOpen(false)}
        onSubmit={handleInsertRow}
      />
    </div>
  );
}
