interface UserPreferences {
  wrapSql: boolean;
  editorFontSize: number;
  autoRefreshTables: boolean;
  showRawResult: boolean;
}

interface PreferencesPanelProps {
  preferences: UserPreferences;
  onChange: (next: UserPreferences) => void;
}

export function PreferencesPanel({ preferences, onChange }: PreferencesPanelProps) {
  return (
    <section className="panel utility-panel">
      <header className="panel-header">
        <h2>User Preferences</h2>
        <p>Adjust editor, result, and browser behavior. Preferences persist locally.</p>
      </header>

      <div className="settings-grid">
        <label className="toggle-row">
          <span>Wrap SQL editor lines</span>
          <input
            type="checkbox"
            checked={preferences.wrapSql}
            onChange={(event) =>
              onChange({ ...preferences, wrapSql: event.target.checked })
            }
          />
        </label>

        <label>
          Editor font size
          <input
            type="range"
            min="12"
            max="20"
            step="1"
            value={preferences.editorFontSize}
            onChange={(event) =>
              onChange({
                ...preferences,
                editorFontSize: Number(event.target.value),
              })
            }
          />
          <span className="setting-caption">{preferences.editorFontSize}px</span>
        </label>

        <label className="toggle-row">
          <span>Auto refresh table browser after writes</span>
          <input
            type="checkbox"
            checked={preferences.autoRefreshTables}
            onChange={(event) =>
              onChange({ ...preferences, autoRefreshTables: event.target.checked })
            }
          />
        </label>

        <label className="toggle-row">
          <span>Show raw JSON under grid results</span>
          <input
            type="checkbox"
            checked={preferences.showRawResult}
            onChange={(event) =>
              onChange({ ...preferences, showRawResult: event.target.checked })
            }
          />
        </label>
      </div>
    </section>
  );
}

export type { UserPreferences };
