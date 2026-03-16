import { TableSchema } from '../api/client';

interface SchemaDiagramProps {
  schema: TableSchema | null;
}

export function SchemaDiagram({ schema }: SchemaDiagramProps) {
  if (!schema) {
    return (
      <section className="panel schema-panel">
        <header className="panel-header">
          <h2>Schema Diagram</h2>
        </header>
        <p className="empty-state">Select a table to view its schema.</p>
      </section>
    );
  }

  return (
    <section className="panel schema-panel">
      <header className="panel-header compact">
        <h2>Schema Diagram</h2>
        <span className="pill">{schema.columns.length} columns</span>
      </header>

      <div className="schema-flow" role="img" aria-label={`Schema for ${schema.name}`}>
        <div className="schema-root">
          <span className="schema-chip">TABLE</span>
          <h3>{schema.name}</h3>
        </div>

        <div className="schema-columns">
          {schema.columns.map((column) => (
            <div key={column.name} className="schema-column">
              <p className="schema-name">{column.name}</p>
              <p className="schema-type">{column.type}</p>
              <span className={column.nullable ? 'schema-null nullable' : 'schema-null'}>
                {column.nullable ? 'nullable' : 'not null'}
              </span>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
