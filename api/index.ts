import { createApp } from './app';
import { getPool } from './pool';

const port = Number(process.env.PORT ?? 3000);
const app = createApp();

const server = app.listen(port, () => {
  console.log(`Advanced Database API listening on http://localhost:${port}`);
});

// Graceful shutdown
const handleShutdown = async (signal: string) => {
  console.log(`\nReceived ${signal}, shutting down gracefully...`);

  // Stop accepting new requests
  server.close(async () => {
    // Drain connection pool
    await getPool().drain();
    process.exit(0);
  });

  // Force exit after 10 seconds
  setTimeout(() => {
    console.error('Forced shutdown after timeout');
    process.exit(1);
  }, 10000);
};

process.on('SIGINT', () => handleShutdown('SIGINT'));
process.on('SIGTERM', () => handleShutdown('SIGTERM'));