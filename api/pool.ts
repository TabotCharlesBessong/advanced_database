import { EngineClient, createEngineClient } from './engineClient';

export interface PoolConfig {
  min?: number;
  max?: number;
  idleTimeout?: number; // ms
}

interface PooledClient {
  client: EngineClient;
  inUse: boolean;
  lastUsedAt: number;
}

/**
 * Connection pool for EngineClient instances.
 * Manages lifecycle of multiple database connections to handle concurrent requests.
 */
export class EngineClientPool {
  private clients: PooledClient[] = [];
  private waiting: Array<(client: EngineClient) => void> = [];
  private config: Required<PoolConfig>;
  private cleanupInterval: NodeJS.Timeout | null = null;

  constructor(config: PoolConfig = {}) {
    this.config = {
      min: config.min ?? 2,
      max: config.max ?? 10,
      idleTimeout: config.idleTimeout ?? 30000, // 30 seconds
    };

    // Initialize minimum pool size
    for (let i = 0; i < this.config.min; i++) {
      this.clients.push({
        client: createEngineClient(),
        inUse: false,
        lastUsedAt: Date.now(),
      });
    }

    // Start cleanup interval
    this.startCleanup();
  }

  /**
   * Acquire a client from the pool.
   * Creates a new client if pool not at max size and all are in use.
   * Waits if pool is at max capacity.
   */
  async acquire(): Promise<EngineClient> {
    // Try to find available client
    const available = this.clients.find((p) => !p.inUse);
    if (available) {
      available.inUse = true;
      available.lastUsedAt = Date.now();
      return available.client;
    }

    // Create new client if under max limit
    if (this.clients.length < this.config.max) {
      const newPooled: PooledClient = {
        client: createEngineClient(),
        inUse: true,
        lastUsedAt: Date.now(),
      };
      this.clients.push(newPooled);
      return newPooled.client;
    }

    // Wait for available client (queue request)
    return new Promise((resolve) => {
      this.waiting.push((client) => {
        const pool = this.clients.find((p) => p.client === client);
        if (pool) {
          pool.inUse = true;
          pool.lastUsedAt = Date.now();
        }
        resolve(client);
      });
    });
  }

  /**
   * Release a client back to the pool
   */
  release(client: EngineClient): void {
    const pooled = this.clients.find((p) => p.client === client);
    if (pooled) {
      pooled.inUse = false;
      pooled.lastUsedAt = Date.now();

      // Notify waiting requests
      const waiter = this.waiting.shift();
      if (waiter) {
        waiter(client);
      }
    }
  }

  /**
   * Get current pool stats
   */
  stats() {
    const inUse = this.clients.filter((p) => p.inUse).length;
    const idle = this.clients.length - inUse;
    const waitingCount = this.waiting.length;

    return {
      totalConnections: this.clients.length,
      inUse,
      idle,
      waitingRequests: waitingCount,
      poolSize: { min: this.config.min, max: this.config.max },
    };
  }

  /**
   * Drain and close all connections (for graceful shutdown)
   */
  async drain(): Promise<void> {
    // Stop accepting new requests
    this.stopCleanup();

    // Wait for in-use clients to be released
    let attempts = 0;
    while (
      this.clients.some((p) => p.inUse) &&
      attempts < 50
    ) {
      await new Promise((resolve) => setTimeout(resolve, 100));
      attempts++;
    }

    // Close all clients
    this.clients.forEach((p) => {
      // EngineClient uses child_process.spawnSync which auto-closes,
      // but could add explicit cleanup here if needed
    });

    this.clients = [];
    this.waiting = [];
  }

  /**
   * Start idle client cleanup interval
   */
  private startCleanup(): void {
    this.cleanupInterval = setInterval(() => {
      const now = Date.now();
      const toRemove: number[] = [];

      this.clients.forEach((p, idx) => {
        // Remove idle clients above minimum pool size
        if (
          !p.inUse &&
          now - p.lastUsedAt > this.config.idleTimeout &&
          this.clients.length > this.config.min
        ) {
          toRemove.push(idx);
        }
      });

      // Remove in reverse order to maintain indices
      for (let i = toRemove.length - 1; i >= 0; i--) {
        this.clients.splice(toRemove[i], 1);
      }
    }, 10000); // Check every 10 seconds
  }

  /**
   * Stop cleanup interval
   */
  private stopCleanup(): void {
    if (this.cleanupInterval) {
      clearInterval(this.cleanupInterval);
      this.cleanupInterval = null;
    }
  }
}

// Global pool instance
let globalPool: EngineClientPool | null = null;

/**
 * Get or create the global connection pool
 */
export function getPool(config?: PoolConfig): EngineClientPool {
  if (!globalPool) {
    globalPool = new EngineClientPool(config);
  }
  return globalPool;
}

/**
 * Reset the global pool (mainly for testing)
 */
export function resetPool(): void {
  if (globalPool) {
    globalPool.drain();
  }
  globalPool = null;
}
