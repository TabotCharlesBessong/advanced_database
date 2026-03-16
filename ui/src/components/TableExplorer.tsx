interface TableExplorerProps {
  tables: string[];
  selectedTable: string | null;
  busy: boolean;
  disabled: boolean;
  onSelect: (tableName: string) => Promise<void>;
  onRefresh: () => Promise<void>;
}

export function TableExplorer({
  tables,
  selectedTable,
  busy,
  disabled,
  onSelect,
  onRefresh,
}: TableExplorerProps) {
  return (
    <section className="panel table-explorer">
      <header className="panel-header compact">
        <h2>Table Explorer</h2>
        <button
          type="button"
          className="secondary"
          onClick={() => {
            void onRefresh();
          }}
          disabled={disabled || busy}
        >
          Refresh
        </button>
      </header>

      {tables.length === 0 ? (
        <p className="empty-state">No tables yet. Run a CREATE TABLE statement to get started.</p>
      ) : (
        <ul className="table-list">
          {tables.map((tableName) => (
            <li key={tableName}>
              <button
                type="button"
                className={selectedTable === tableName ? 'table-item active' : 'table-item'}
                onClick={() => {
                  void onSelect(tableName);
                }}
                disabled={disabled || busy}
              >
                {tableName}
              </button>
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
