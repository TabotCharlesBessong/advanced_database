interface QueryResultProps {
  result: unknown;
}

export function QueryResult({ result }: QueryResultProps) {
  return (
    <section className="panel result-panel">
      <header className="panel-header">
        <h2>Query Result</h2>
      </header>

      <pre className="result-view">{JSON.stringify(result, null, 2)}</pre>
    </section>
  );
}
