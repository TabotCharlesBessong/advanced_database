import { FormEvent, useState } from 'react';
import { ConnectionConfig } from '../api/client';

type AuthMode = 'login' | 'signup';

interface ConnectionPanelProps {
  busy: boolean;
  isConnected: boolean;
  onConnect: (config: ConnectionConfig) => Promise<void>;
  onSignup: (config: ConnectionConfig) => Promise<void>;
  onDisconnect: () => void;
  defaultUrl: string;
}

export function ConnectionPanel({
  busy,
  isConnected,
  onConnect,
  onSignup,
  onDisconnect,
  defaultUrl,
}: ConnectionPanelProps) {
  const [baseUrl, setBaseUrl] = useState(defaultUrl);
  const [username, setUsername] = useState('admin');
  const [password, setPassword] = useState('password123');
  const [confirmPassword, setConfirmPassword] = useState('password123');
  const [passwordVisible, setPasswordVisible] = useState(false);
  const [confirmPasswordVisible, setConfirmPasswordVisible] = useState(false);
  const [authMode, setAuthMode] = useState<AuthMode>('login');

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();

    const config = {
      baseUrl: baseUrl.trim(),
      username: username.trim(),
      password,
    };

    if (authMode === 'signup') {
      if (password !== confirmPassword) {
        return;
      }
      await onSignup(config);
      return;
    }

    await onConnect(config);
  };

  return (
    <section className="panel connection-panel">
      <header className="panel-header">
        <h2>Connection</h2>
        <span className={isConnected ? 'badge badge-on' : 'badge badge-off'}>
          {isConnected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      {!isConnected && (
        <div className="onboarding">
          <p>Onboarding</p>
          <ol>
            <li>Create an account using Sign Up.</li>
            <li>Sign in and initialize your workspace.</li>
            <li>Run SQL in the query panel.</li>
          </ol>
        </div>
      )}

      <form className="form" onSubmit={handleSubmit}>
        {!isConnected && (
          <div className="auth-mode-toggle" role="tablist" aria-label="Authentication mode">
            <button
              type="button"
              className={authMode === 'login' ? 'secondary active' : 'secondary'}
              onClick={() => setAuthMode('login')}
              disabled={busy}
            >
              Login
            </button>
            <button
              type="button"
              className={authMode === 'signup' ? 'secondary active' : 'secondary'}
              onClick={() => setAuthMode('signup')}
              disabled={busy}
            >
              Sign Up
            </button>
          </div>
        )}

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
          <div className="password-field">
            <input
              type={passwordVisible ? 'text' : 'password'}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              required
              minLength={6}
              disabled={busy || isConnected}
            />
            <button
              type="button"
              className="inline-toggle"
              onClick={() => setPasswordVisible((prev) => !prev)}
              disabled={busy || isConnected}
            >
              {passwordVisible ? 'Hide' : 'Show'}
            </button>
          </div>
        </label>

        {authMode === 'signup' && !isConnected && (
          <label>
            Confirm password
            <div className="password-field">
              <input
                type={confirmPasswordVisible ? 'text' : 'password'}
                value={confirmPassword}
                onChange={(e) => setConfirmPassword(e.target.value)}
                required
                minLength={6}
                disabled={busy || isConnected}
              />
              <button
                type="button"
                className="inline-toggle"
                onClick={() => setConfirmPasswordVisible((prev) => !prev)}
                disabled={busy || isConnected}
              >
                {confirmPasswordVisible ? 'Hide' : 'Show'}
              </button>
            </div>
            {password !== confirmPassword && (
              <span className="error-text">Passwords do not match.</span>
            )}
          </label>
        )}

        <div className="button-row">
          {!isConnected ? (
            <button type="submit" disabled={busy}>
              {busy ? 'Working...' : authMode === 'signup' ? 'Create Account' : 'Connect'}
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
