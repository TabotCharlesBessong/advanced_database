interface QueryHistoryEntry {
  id: string;
  sql: string;
  executedAt: string;
  status: 'success' | 'error';
  durationMs: number;
}

interface FavoriteQuery {
  id: string;
  name: string;
  sql: string;
  createdAt: string;
}

interface QueryLibraryProps {
  currentSql: string;
  favorites: FavoriteQuery[];
  history: QueryHistoryEntry[];
  busy: boolean;
  onLoadQuery: (sql: string) => void;
  onSaveFavorite: (name: string) => void;
  onRemoveFavorite: (id: string) => void;
  onClearHistory: () => void;
}

export function QueryLibrary({
  currentSql,
  favorites,
  history,
  busy,
  onLoadQuery,
  onSaveFavorite,
  onRemoveFavorite,
  onClearHistory,
}: QueryLibraryProps) {
  const suggestedName = currentSql
    .trim()
    .split(/\s+/)
    .slice(0, 4)
    .join(' ')
    .replace(/[^a-zA-Z0-9_ ]/g, '')
    .slice(0, 28);

  return (
    <section className="panel utility-panel">
      <header className="panel-header compact">
        <h2>Query Library</h2>
        <button
          type="button"
          className="secondary"
          disabled={busy || currentSql.trim().length === 0}
          onClick={() => onSaveFavorite(suggestedName || 'Saved query')}
        >
          Save Favorite
        </button>
      </header>

      <div className="library-section">
        <h3>Favorites</h3>
        {favorites.length === 0 ? (
          <p className="empty-state">Save frequently used SQL for quick reuse.</p>
        ) : (
          <ul className="stack-list">
            {favorites.map((favorite) => (
              <li key={favorite.id} className="stack-item">
                <div>
                  <strong>{favorite.name}</strong>
                  <p>{favorite.sql}</p>
                </div>
                <div className="button-row">
                  <button
                    type="button"
                    className="secondary"
                    onClick={() => onLoadQuery(favorite.sql)}
                    disabled={busy}
                  >
                    Load
                  </button>
                  <button
                    type="button"
                    className="secondary danger"
                    onClick={() => onRemoveFavorite(favorite.id)}
                    disabled={busy}
                  >
                    Remove
                  </button>
                </div>
              </li>
            ))}
          </ul>
        )}
      </div>

      <div className="library-section">
        <div className="panel-header compact no-margin">
          <h3>History</h3>
          <button type="button" className="secondary" onClick={onClearHistory} disabled={busy || history.length === 0}>
            Clear
          </button>
        </div>

        {history.length === 0 ? (
          <p className="empty-state">Executed queries will appear here.</p>
        ) : (
          <ul className="stack-list compact-list">
            {history.map((entry) => (
              <li key={entry.id} className="stack-item history-item">
                <div>
                  <p className="history-sql">{entry.sql}</p>
                  <span className={entry.status === 'success' ? 'mini-badge success' : 'mini-badge error'}>
                    {entry.status} · {entry.durationMs} ms
                  </span>
                </div>
                <button
                  type="button"
                  className="secondary"
                  onClick={() => onLoadQuery(entry.sql)}
                  disabled={busy}
                >
                  Reuse
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}

export type { FavoriteQuery, QueryHistoryEntry };
