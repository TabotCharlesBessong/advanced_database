import { useMemo, useState } from 'react';
import {
  ConnectionConfig,
  executeSql,
  initializeDatabase,
  login,
} from './api/client';
import { ConnectionPanel } from './components/ConnectionPanel';
import { QueryResult } from './components/QueryResult';
import { SqlEditor } from './components/SqlEditor';

type ActiveView = 'query' | 'docs';

interface SessionState {
  baseUrl: string;
  token: string;
  username: string;
}

const DEFAULT_API_URL = 'http://localhost:3000';

export function App() {
  const [activeView, setActiveView] = useState<ActiveView>('query');
  const [busy, setBusy] = useState(false);
  const [session, setSession] = useState<SessionState | null>(null);
  const [queryResult, setQueryResult] = useState<unknown>(null);
  const [statusMessage, setStatusMessage] = useState('Not connected.');

  const connected = Boolean(session);

  const docsUrl = useMemo(() => {
    if (session) {
      return `${session.baseUrl}/api-docs`;
    }
    return `${DEFAULT_API_URL}/api-docs`;
  }, [session]);

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
    setBusy(false);
  };

  const handleDisconnect = () => {
    setSession(null);
    setQueryResult(null);
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
    setBusy(false);
  };

  return (
    <div className="app-shell">
      <div className="ambient-shape ambient-one" />
      <div className="ambient-shape ambient-two" />

      <header className="topbar">
        <div>
          <p className="eyebrow">Phase 6 • Week 29-30</p>
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
    </div>
  );
}
