interface DatabaseSelectorProps {
  databases: string[];
  currentDatabase: string | null;
  busy: boolean;
  disabled: boolean;
  onRefresh: () => Promise<void>;
  onSelect: (databaseName: string) => Promise<void>;
}

export function DatabaseSelector({
  databases,
  currentDatabase,
  busy,
  disabled,
  onRefresh,
  onSelect,
}: DatabaseSelectorProps) {
  return (
    <section className="panel utility-panel">
      <header className="panel-header compact">
        <h2>Databases</h2>
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

      {databases.length === 0 ? (
        <p className="empty-state">No databases discovered yet.</p>
      ) : (
        <div className="database-list">
          {databases.map((databaseName) => (
            <button
              key={databaseName}
              type="button"
              className={currentDatabase === databaseName ? 'table-item active' : 'table-item'}
              onClick={() => {
                void onSelect(databaseName);
              }}
              disabled={disabled || busy}
            >
              {databaseName}
            </button>
          ))}
        </div>
      )}
    </section>
  );
}
