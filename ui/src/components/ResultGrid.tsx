interface ResultGridProps {
  rows: Array<Record<string, unknown>>;
  title?: string;
}

function buildColumns(rows: Array<Record<string, unknown>>): string[] {
  const seen = new Set<string>();
  const columns: string[] = [];

  for (const row of rows) {
    for (const key of Object.keys(row)) {
      if (!seen.has(key)) {
        seen.add(key);
        columns.push(key);
      }
    }
  }

  return columns;
}

function formatCell(value: unknown): string {
  if (value === null) {
    return 'NULL';
  }

  if (typeof value === 'object') {
    return JSON.stringify(value);
  }

  return String(value);
}

export function ResultGrid({ rows, title = 'Result Grid' }: ResultGridProps) {
  if (rows.length === 0) {
    return (
      <section className="panel result-panel">
        <header className="panel-header compact">
          <h2>{title}</h2>
          <span className="pill">0 rows</span>
        </header>
        <p className="empty-state">No rows found.</p>
      </section>
    );
  }

  const columns = buildColumns(rows);

  return (
    <section className="panel result-panel">
      <header className="panel-header compact">
        <h2>{title}</h2>
        <span className="pill">{rows.length} rows</span>
      </header>

      <div className="grid-wrap">
        <table className="data-grid">
          <thead>
            <tr>
              {columns.map((column) => (
                <th key={column}>{column}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((row, index) => (
              <tr key={`${index}-${columns.join('|')}`}>
                {columns.map((column) => (
                  <td key={column}>{formatCell(row[column])}</td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}
