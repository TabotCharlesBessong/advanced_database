import { FormEvent } from 'react';

interface SqlEditorProps {
  busy: boolean;
  disabled: boolean;
  sql: string;
  wrapSql: boolean;
  fontSize: number;
  onChange: (sql: string) => void;
  onRun: (sql: string) => Promise<void>;
}

export function SqlEditor({
  busy,
  disabled,
  sql,
  wrapSql,
  fontSize,
  onChange,
  onRun,
}: SqlEditorProps) {
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
          onChange={(e) => onChange(e.target.value)}
          spellCheck={false}
          disabled={disabled || busy}
          placeholder="Write SQL here..."
          aria-label="SQL editor"
          style={{ fontSize: `${fontSize}px`, whiteSpace: wrapSql ? 'pre-wrap' : 'pre' }}
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
