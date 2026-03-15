interface PerformanceMetric {
  id: string;
  operation: string;
  durationMs: number;
  ok: boolean;
  timestamp: string;
  statusCode?: number;
}

interface PerformanceDashboardProps {
  metrics: PerformanceMetric[];
}

function average(values: number[]): number {
  if (values.length === 0) {
    return 0;
  }

  return Math.round(values.reduce((sum, value) => sum + value, 0) / values.length);
}

export function PerformanceDashboard({ metrics }: PerformanceDashboardProps) {
  const durations = metrics.map((metric) => metric.durationMs);
  const successes = metrics.filter((metric) => metric.ok).length;
  const failureCount = metrics.length - successes;
  const averageDuration = average(durations);
  const slowestDuration = durations.length === 0 ? 0 : Math.max(...durations);

  return (
    <section className="panel utility-panel">
      <header className="panel-header">
        <h2>Performance Dashboard</h2>
        <p>Recent client-side request timings for the current studio session.</p>
      </header>

      <div className="metric-cards">
        <div className="metric-card">
          <span>Total requests</span>
          <strong>{metrics.length}</strong>
        </div>
        <div className="metric-card">
          <span>Successes</span>
          <strong>{successes}</strong>
        </div>
        <div className="metric-card">
          <span>Failures</span>
          <strong>{failureCount}</strong>
        </div>
        <div className="metric-card">
          <span>Average latency</span>
          <strong>{averageDuration} ms</strong>
        </div>
        <div className="metric-card">
          <span>Slowest request</span>
          <strong>{slowestDuration} ms</strong>
        </div>
      </div>

      {metrics.length === 0 ? (
        <p className="empty-state">Run API actions to populate performance metrics.</p>
      ) : (
        <div className="grid-wrap">
          <table className="data-grid compact-grid">
            <thead>
              <tr>
                <th>Operation</th>
                <th>Status</th>
                <th>Latency</th>
                <th>HTTP</th>
                <th>Time</th>
              </tr>
            </thead>
            <tbody>
              {metrics.map((metric) => (
                <tr key={metric.id}>
                  <td>{metric.operation}</td>
                  <td>
                    <span className={metric.ok ? 'mini-badge success' : 'mini-badge error'}>
                      {metric.ok ? 'success' : 'error'}
                    </span>
                  </td>
                  <td>{metric.durationMs} ms</td>
                  <td>{metric.statusCode ?? 'n/a'}</td>
                  <td>{new Date(metric.timestamp).toLocaleTimeString()}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}

export type { PerformanceMetric };
