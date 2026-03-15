import { ResultGrid } from './ResultGrid';

interface QueryResultProps {
  result: unknown;
}

function hasRowArray(
  value: unknown
): value is { rows: Array<Record<string, unknown>> } {
  if (!value || typeof value !== 'object') {
    return false;
  }

  const candidate = value as { rows?: unknown };
  return Array.isArray(candidate.rows);
}

export function QueryResult({ result }: QueryResultProps) {
  const rows = hasRowArray(result) ? result.rows : null;

  return (
    <>
      {rows && <ResultGrid rows={rows} title="Query Result Grid" />}

      <section className="panel result-panel">
        <header className="panel-header compact">
          <h2>Raw Result</h2>
        </header>

        <pre className="result-view">{JSON.stringify(result, null, 2)}</pre>
      </section>
    </>
  );
}
