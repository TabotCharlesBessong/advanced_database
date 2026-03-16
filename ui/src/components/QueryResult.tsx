import { ResultGrid } from './ResultGrid';

interface QueryResultProps {
  result: unknown;
  showRawResult?: boolean;
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

export function QueryResult({ result, showRawResult = true }: QueryResultProps) {
  const rows = hasRowArray(result) ? result.rows : null;

  return (
    <>
      {rows && <ResultGrid rows={rows} title="Query Result Grid" />}

      {showRawResult && (
        <section className="panel result-panel">
          <header className="panel-header compact">
            <h2>Raw Result</h2>
          </header>

          <pre className="result-view">{JSON.stringify(result, null, 2)}</pre>
        </section>
      )}
    </>
  );
}
