import { FormEvent, useRef } from 'react';

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
  const editorRef = useRef<HTMLTextAreaElement | null>(null);

  const handleSubmit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    await onRun(sql);
  };

  const handleRunSelection = async () => {
    const editor = editorRef.current;
    if (!editor) {
      await onRun(sql);
      return;
    }

    const selection = editor.value
      .slice(editor.selectionStart, editor.selectionEnd)
      .trim();

    await onRun(selection.length > 0 ? selection : sql);
  };

  return (
    <section className="panel sql-panel">
      <header className="panel-header">
        <h2>SQL Editor</h2>
        <p>Run read or write statements through the Phase 5 API.</p>
      </header>

      <form onSubmit={handleSubmit} className="sql-form">
        <textarea
          ref={editorRef}
          value={sql}
          onChange={(e) => onChange(e.target.value)}
          onKeyDown={(event) => {
            if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
              event.preventDefault();
              void handleRunSelection();
            }
          }}
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
          <button
            type="button"
            className="secondary"
            onClick={() => {
              void handleRunSelection();
            }}
            disabled={disabled || busy || sql.trim().length === 0}
          >
            Run Selection
          </button>
        </div>
      </form>
    </section>
  );
}
