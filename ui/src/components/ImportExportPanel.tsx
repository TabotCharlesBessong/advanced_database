interface ImportExportPanelProps {
  canExportRows: boolean;
  disabled: boolean;
  onImportSql: (file: File) => Promise<void>;
  onExportJson: () => void;
  onExportCsv: () => void;
}

export function ImportExportPanel({
  canExportRows,
  disabled,
  onImportSql,
  onExportJson,
  onExportCsv,
}: ImportExportPanelProps) {
  return (
    <section className="panel utility-panel">
      <header className="panel-header">
        <h2>Import / Export</h2>
        <p>Load SQL scripts from disk or export query output for analysis.</p>
      </header>

      <div className="import-box">
        <label className="file-field">
          <span>Import SQL file</span>
          <input
            type="file"
            accept=".sql,text/plain"
            disabled={disabled}
            onChange={(event) => {
              const file = event.target.files?.[0];
              if (!file) {
                return;
              }
              void onImportSql(file);
              event.currentTarget.value = '';
            }}
          />
        </label>
      </div>

      <div className="button-row">
        <button type="button" className="secondary" onClick={onExportJson} disabled={disabled}>
          Export JSON
        </button>
        <button type="button" className="secondary" onClick={onExportCsv} disabled={disabled || !canExportRows}>
          Export CSV
        </button>
      </div>
    </section>
  );
}
