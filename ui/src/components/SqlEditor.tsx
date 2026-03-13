import { FormEvent, useState } from 'react';

interface SqlEditorProps {
  busy: boolean;
  disabled: boolean;
  onRun: (sql: string) => Promise<void>;
}

const STARTER_SQL = `SELECT * FROM users;`;

export function SqlEditor({ busy, disabled, onRun }: SqlEditorProps) {
  const [sql, setSql] = useState(STARTER_SQL);

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    await onRun(sql);
  };

  return (
    <section className="panel sql-panel">
      <header className="panel-header">
        <h2>SQL Editor</h2>
        <p>Run read or write statements through the Phase 5 API.</p>
      </header>

      <form onSubmit={handleSubmit} className="sql-form">
        <textarea
          value={sql}
          onChange={(e) => setSql(e.target.value)}
          spellCheck={false}
          disabled={disabled || busy}
          placeholder="Write SQL here..."
          aria-label="SQL editor"
        />

        <div className="button-row">
          <button type="submit" disabled={disabled || busy || sql.trim().length === 0}>
            {busy ? 'Running...' : 'Run Query'}
          </button>
        </div>
      </form>
    </section>
  );
}
