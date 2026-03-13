import { FormEvent, useState } from 'react';
import { ConnectionConfig } from '../api/client';

interface ConnectionPanelProps {
  busy: boolean;
  isConnected: boolean;
  onConnect: (config: ConnectionConfig) => Promise<void>;
  onDisconnect: () => void;
  defaultUrl: string;
}

export function ConnectionPanel({
  busy,
  isConnected,
  onConnect,
  onDisconnect,
  defaultUrl,
}: ConnectionPanelProps) {
  const [baseUrl, setBaseUrl] = useState(defaultUrl);
  const [username, setUsername] = useState('admin');
  const [password, setPassword] = useState('password123');

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    await onConnect({ baseUrl: baseUrl.trim(), username: username.trim(), password });
  };

  return (
    <section className="panel connection-panel">
      <header className="panel-header">
        <h2>Connection</h2>
        <span className={isConnected ? 'badge badge-on' : 'badge badge-off'}>
          {isConnected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      <form className="form" onSubmit={handleSubmit}>
        <label>
          API URL
          <input
            type="url"
            value={baseUrl}
            onChange={(e) => setBaseUrl(e.target.value)}
            placeholder="http://localhost:3000"
            required
            disabled={busy || isConnected}
          />
        </label>

        <label>
          Username
          <input
            type="text"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            required
            disabled={busy || isConnected}
          />
        </label>

        <label>
          Password
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required
            disabled={busy || isConnected}
          />
        </label>

        <div className="button-row">
          {!isConnected ? (
            <button type="submit" disabled={busy}>
              {busy ? 'Connecting...' : 'Connect'}
            </button>
          ) : (
            <button type="button" className="secondary" onClick={onDisconnect} disabled={busy}>
              Disconnect
            </button>
          )}
        </div>
      </form>
    </section>
  );
}
