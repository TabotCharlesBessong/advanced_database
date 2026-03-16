import { ErRelation, TableSchema } from '../api/client';

interface ErDiagramProps {
  databaseName: string | null;
  tables: TableSchema[];
  relations: ErRelation[];
}

export function ErDiagram({ databaseName, tables, relations }: ErDiagramProps) {
  return (
    <section className="panel er-panel">
      <header className="panel-header compact">
        <h2>ER Diagram</h2>
        <span className="pill">{databaseName ?? 'n/a'}</span>
      </header>

      {tables.length === 0 ? (
        <p className="empty-state">Create tables in the selected database to build an ER view.</p>
      ) : (
        <div className="er-layout">
          <div className="er-tables">
            {tables.map((table) => (
              <article key={table.name} className="er-table-card">
                <h3>{table.name}</h3>
                <ul>
                  {table.columns.map((column) => (
                    <li key={column.name}>
                      <strong>{column.name}</strong>
                      <span>{column.type}</span>
                    </li>
                  ))}
                </ul>
              </article>
            ))}
          </div>

          <div className="er-relations">
            <h3>Relationships</h3>
            {relations.length === 0 ? (
              <p className="empty-state">No relationships inferred yet.</p>
            ) : (
              <ul className="stack-list compact-list">
                {relations.map((relation, index) => (
                  <li key={`${relation.fromTable}-${relation.fromColumn}-${index}`} className="stack-item">
                    <p className="relation-line">
                      {relation.fromTable}.{relation.fromColumn} → {relation.toTable}.{relation.toColumn}
                    </p>
                    <span className="mini-badge success">{relation.kind}</span>
                  </li>
                ))}
              </ul>
            )}
          </div>
        </div>
      )}
    </section>
  );
}
