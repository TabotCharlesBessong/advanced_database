import { useEffect, useMemo, useState } from 'react';
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
import { ImportExportPanel } from './components/ImportExportPanel';
import { PerformanceDashboard, PerformanceMetric } from './components/PerformanceDashboard';
import { PreferencesPanel, UserPreferences } from './components/PreferencesPanel';
import { QueryLibrary, FavoriteQuery, QueryHistoryEntry } from './components/QueryLibrary';
import { QueryResult } from './components/QueryResult';
import { ResultGrid } from './components/ResultGrid';
import { RowEditorDialog } from './components/RowEditorDialog';
import { SchemaDiagram } from './components/SchemaDiagram';
import { SqlEditor } from './components/SqlEditor';
import { TableExplorer } from './components/TableExplorer';

type ActiveView = 'query' | 'browser' | 'insights' | 'docs';

interface SessionState {
  baseUrl: string;
  token: string;
  username: string;
}

const DEFAULT_API_URL = 'http://localhost:3000';
const STARTER_SQL = 'SELECT * FROM users;';
const SQL_DRAFT_KEY = 'advanced-db-sql-draft';
const QUERY_HISTORY_KEY = 'advanced-db-query-history';
const QUERY_FAVORITES_KEY = 'advanced-db-query-favorites';
const PREFERENCES_KEY = 'advanced-db-preferences';
const PERFORMANCE_KEY = 'advanced-db-performance';

const DEFAULT_PREFERENCES: UserPreferences = {
  wrapSql: false,
  editorFontSize: 14,
  autoRefreshTables: true,
  showRawResult: true,
};

function readStoredJson<T>(key: string, fallback: T): T {
  try {
    const raw = window.localStorage.getItem(key);
    if (!raw) {
      return fallback;
    }
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

function writeStoredJson<T>(key: string, value: T) {
  window.localStorage.setItem(key, JSON.stringify(value));
}

function extractRows(result: unknown): Array<Record<string, unknown>> {
  if (!result || typeof result !== 'object') {
    return [];
  }

  const candidate = result as { rows?: unknown };
  return Array.isArray(candidate.rows)
    ? (candidate.rows as Array<Record<string, unknown>>)
    : [];
}

function escapeCsv(value: unknown): string {
  if (value === null || value === undefined) {
    return '';
  }

  const text = typeof value === 'object' ? JSON.stringify(value) : String(value);
  return `"${text.replace(/"/g, '""')}"`;
}

function rowsToCsv(rows: Array<Record<string, unknown>>): string {
  const columnSet = new Set<string>();
  for (const row of rows) {
    Object.keys(row).forEach((key) => columnSet.add(key));
  }

  const columns = Array.from(columnSet);
  const header = columns.join(',');
  const body = rows
    .map((row) => columns.map((column) => escapeCsv(row[column])).join(','))
    .join('\n');

  return [header, body].filter((part) => part.length > 0).join('\n');
}

function downloadTextFile(fileName: string, content: string, mimeType: string) {
  const blob = new Blob([content], { type: mimeType });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = fileName;
  document.body.appendChild(anchor);
  anchor.click();
  document.body.removeChild(anchor);
  URL.revokeObjectURL(url);
}

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
  const [sqlDraft, setSqlDraft] = useState<string>(() => readStoredJson(SQL_DRAFT_KEY, STARTER_SQL));
  const [queryHistory, setQueryHistory] = useState<QueryHistoryEntry[]>(() =>
    readStoredJson<QueryHistoryEntry[]>(QUERY_HISTORY_KEY, [])
  );
  const [favoriteQueries, setFavoriteQueries] = useState<FavoriteQuery[]>(() =>
    readStoredJson<FavoriteQuery[]>(QUERY_FAVORITES_KEY, [])
  );
  const [preferences, setPreferences] = useState<UserPreferences>(() =>
    readStoredJson<UserPreferences>(PREFERENCES_KEY, DEFAULT_PREFERENCES)
  );
  const [performanceMetrics, setPerformanceMetrics] = useState<PerformanceMetric[]>(() =>
    readStoredJson<PerformanceMetric[]>(PERFORMANCE_KEY, [])
  );

  const connected = Boolean(session);

  useEffect(() => {
    writeStoredJson(SQL_DRAFT_KEY, sqlDraft);
  }, [sqlDraft]);

  useEffect(() => {
    writeStoredJson(QUERY_HISTORY_KEY, queryHistory);
  }, [queryHistory]);

  useEffect(() => {
    writeStoredJson(QUERY_FAVORITES_KEY, favoriteQueries);
  }, [favoriteQueries]);

  useEffect(() => {
    writeStoredJson(PREFERENCES_KEY, preferences);
  }, [preferences]);

  useEffect(() => {
    writeStoredJson(PERFORMANCE_KEY, performanceMetrics);
  }, [performanceMetrics]);

  const docsUrl = useMemo(() => {
    if (session) {
      return `${session.baseUrl}/api-docs`;
    }
    return `${DEFAULT_API_URL}/api-docs`;
  }, [session]);

  const queryRows = useMemo(() => extractRows(queryResult), [queryResult]);

  const recordMetric = (
    operation: string,
    result: { ok: boolean; durationMs: number; statusCode?: number }
  ) => {
    const metric: PerformanceMetric = {
      id: crypto.randomUUID(),
      operation,
      durationMs: result.durationMs,
      ok: result.ok,
      statusCode: result.statusCode,
      timestamp: new Date().toISOString(),
    };

    setPerformanceMetrics((previous) => [metric, ...previous].slice(0, 30));
  };

  const addHistoryEntry = (
    sql: string,
    result: { ok: boolean; durationMs: number }
  ) => {
    const entry: QueryHistoryEntry = {
      id: crypto.randomUUID(),
      sql: sql.trim(),
      executedAt: new Date().toISOString(),
      status: result.ok ? 'success' : 'error',
      durationMs: result.durationMs,
    };

    setQueryHistory((previous) => [entry, ...previous].slice(0, 20));
  };

  const loadTableDetails = async (baseUrl: string, token: string, tableName: string) => {
    setBrowserBusy(true);
    setSelectedTable(tableName);

    const [schemaResponse, rowsResponse] = await Promise.all([
      describeTable(baseUrl, token, tableName),
      getTableRows(baseUrl, token, tableName),
    ]);

    recordMetric('describe_table', schemaResponse);
    recordMetric('get_table_rows', rowsResponse);

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

  const refreshTables = async (baseUrl: string, token: string) => {
    setBrowserBusy(true);
    const response = await listTables(baseUrl, token);
    recordMetric('list_tables', response);

    if (!response.ok || !response.data) {
      setStatusMessage(`Failed to load tables: ${response.error}`);
      setTables([]);
      setBrowserBusy(false);
      return;
    }

    setTables(response.data.tables);
    if (response.data.tables.length === 0) {
      setSelectedTable(null);
      setSelectedSchema(null);
      setSelectedRows([]);
      setStatusMessage('No tables found yet.');
      setBrowserBusy(false);
      return;
    }

    if (!selectedTable || !response.data.tables.includes(selectedTable)) {
      await loadTableDetails(baseUrl, token, response.data.tables[0]);
    } else {
      setStatusMessage(`Loaded ${response.data.tables.length} table(s).`);
      setBrowserBusy(false);
    }
  };

  const handleConnect = async (config: ConnectionConfig) => {
    setBusy(true);
    setStatusMessage('Authenticating...');

    const loginResponse = await login(config);
    recordMetric('login', loginResponse);
    if (!loginResponse.ok || !loginResponse.data) {
      setStatusMessage(`Connection failed: ${loginResponse.error}`);
      setBusy(false);
      return;
    }

    setStatusMessage('Initializing database...');
    const initResponse = await initializeDatabase(config.baseUrl, loginResponse.data.token);
    recordMetric('init', initResponse);
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
    recordMetric('signup', signupResponse);
    if (!signupResponse.ok || !signupResponse.data) {
      setStatusMessage(`Signup failed: ${signupResponse.error}`);
      setBusy(false);
      return;
    }

    setStatusMessage('Account created. Initializing database...');
    const initResponse = await initializeDatabase(config.baseUrl, signupResponse.data.token);
    recordMetric('init', initResponse);
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
    recordMetric('execute_sql', response);
    addHistoryEntry(sql, response);

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

    if (preferences.autoRefreshTables && (statement === 'create_table' || statement === 'insert')) {
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
    recordMetric('insert_row', response);

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

  const handleLoadQuery = (sql: string) => {
    setSqlDraft(sql);
    setActiveView('query');
    setStatusMessage('Loaded query into editor.');
  };

  const handleSaveFavorite = (name: string) => {
    const trimmedSql = sqlDraft.trim();
    if (trimmedSql.length === 0) {
      setStatusMessage('Cannot save an empty query.');
      return;
    }

    const trimmedName = name.trim() || 'Saved query';
    const favorite: FavoriteQuery = {
      id: crypto.randomUUID(),
      name: trimmedName,
      sql: trimmedSql,
      createdAt: new Date().toISOString(),
    };

    setFavoriteQueries((previous) => [favorite, ...previous].slice(0, 12));
    setStatusMessage(`Saved favorite: ${trimmedName}.`);
  };

  const handleRemoveFavorite = (id: string) => {
    setFavoriteQueries((previous) => previous.filter((favorite) => favorite.id !== id));
    setStatusMessage('Removed favorite query.');
  };

  const handleClearHistory = () => {
    setQueryHistory([]);
    setStatusMessage('Cleared query history.');
  };

  const handleImportSql = async (file: File) => {
    const text = await file.text();
    setSqlDraft(text);
    setActiveView('query');
    setStatusMessage(`Imported SQL from ${file.name}.`);
  };

  const handleExportJson = () => {
    if (queryResult === null) {
      setStatusMessage('Run a query first to export results.');
      return;
    }

    downloadTextFile(
      `query-result-${Date.now()}.json`,
      JSON.stringify(queryResult, null, 2),
      'application/json'
    );
    setStatusMessage('Exported current result as JSON.');
  };

  const handleExportCsv = () => {
    if (queryRows.length === 0) {
      setStatusMessage('CSV export requires a result set with rows.');
      return;
    }

    downloadTextFile(`query-result-${Date.now()}.csv`, rowsToCsv(queryRows), 'text/csv');
    setStatusMessage('Exported current result as CSV.');
  };

  return (
    <div className="app-shell">
      <div className="ambient-shape ambient-one" />
      <div className="ambient-shape ambient-two" />

      <header className="topbar">
        <div>
          <p className="eyebrow">Phase 6 • Week 33-34</p>
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
            className={activeView === 'insights' ? 'tab active' : 'tab'}
            onClick={() => setActiveView('insights')}
          >
            Insights
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
              <li>History: {queryHistory.length} items</li>
              <li>Favorites: {favoriteQueries.length} items</li>
            </ul>
          </section>
        </aside>

        <section className="content">
          {activeView === 'query' ? (
            <section className="query-workspace">
              <div className="query-main">
                <SqlEditor
                  busy={busy}
                  disabled={!connected}
                  sql={sqlDraft}
                  wrapSql={preferences.wrapSql}
                  fontSize={preferences.editorFontSize}
                  onChange={setSqlDraft}
                  onRun={handleRunSql}
                />
                {queryResult !== null && (
                  <QueryResult result={queryResult} showRawResult={preferences.showRawResult} />
                )}
              </div>

              <div className="query-sidepanels">
                <QueryLibrary
                  currentSql={sqlDraft}
                  favorites={favoriteQueries}
                  history={queryHistory}
                  busy={busy}
                  onLoadQuery={handleLoadQuery}
                  onSaveFavorite={handleSaveFavorite}
                  onRemoveFavorite={handleRemoveFavorite}
                  onClearHistory={handleClearHistory}
                />
                <ImportExportPanel
                  canExportRows={queryRows.length > 0}
                  disabled={busy}
                  onImportSql={handleImportSql}
                  onExportJson={handleExportJson}
                  onExportCsv={handleExportCsv}
                />
              </div>
            </section>
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
          ) : activeView === 'insights' ? (
            <section className="insights-layout">
              <PreferencesPanel preferences={preferences} onChange={setPreferences} />
              <PerformanceDashboard metrics={performanceMetrics} />
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
