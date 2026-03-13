import { randomUUID } from 'crypto';
import { EngineClient } from './engineClient';

export interface PreparedStatement {
  id: string;
  sql: string;
  paramCount: number;
  paramTypes: string[]; // 'string', 'number', 'boolean'
}

export interface ExecutePreparedParams {
  statementId: string;
  parameters: (string | number | boolean | null)[];
}

/**
 * Prepared statement registry for managing cached statements per connection
 */
export class PreparedStatementRegistry {
  private statements = new Map<string, PreparedStatement>();
  private parameterizedSqlCache = new Map<string, string>();

  /**
   * Parse SQL string and extract parameter placeholders (?)
   * @param sql Raw SQL with ? placeholders
   * @returns Prepared statement with parameter count
   */
  prepare(sql: string): PreparedStatement {
    // Check cache first
    const cached = this.statements.get(sql);
    if (cached) {
      return cached;
    }

    // Count parameter placeholders (?)
    const paramCount = (sql.match(/\?/g) || []).length;
    const paramTypes = Array(paramCount).fill('unknown');

    const stmt: PreparedStatement = {
      id: randomUUID(),
      sql,
      paramCount,
      paramTypes,
    };

    this.statements.set(sql, stmt);
    return stmt;
  }

  /**
   * Execute a prepared statement by binding parameters
   * @param stmt The prepared statement
   * @param parameters Parameter values to bind
   * @returns Final SQL string with parameters inserted
   */
  bindParameters(
    stmt: PreparedStatement,
    parameters: (string | number | boolean | null)[]
  ): string {
    if (parameters.length !== stmt.paramCount) {
      throw new Error(
        `Expected ${stmt.paramCount} parameters, got ${parameters.length}`
      );
    }

    let result = stmt.sql;
    let paramIndex = 0;

    // Replace ? placeholders with escaped parameter values
    result = result.replace(/\?/g, () => {
      if (paramIndex >= parameters.length) {
        throw new Error('Not enough parameters provided');
      }

      const param = parameters[paramIndex];
      paramIndex++;

      return this.escapeParameter(param);
    });

    return result;
  }

  /**
   * Escape parameter value for SQL injection prevention
   */
  private escapeParameter(param: string | number | boolean | null): string {
    if (param === null) {
      return 'NULL';
    }

    if (typeof param === 'number') {
      return param.toString();
    }

    if (typeof param === 'boolean') {
      return param ? '1' : '0';
    }

    // String: escape single quotes and wrap in quotes
    const escaped = param.replace(/'/g, "''");
    return `'${escaped}'`;
  }

  /**
   * Get a prepared statement by ID
   */
  getStatement(id: string): PreparedStatement | undefined {
    for (const stmt of this.statements.values()) {
      if (stmt.id === id) {
        return stmt;
      }
    }
    return undefined;
  }

  /**
   * Clear all cached statements (for memory management)
   */
  clear(): void {
    this.statements.clear();
    this.parameterizedSqlCache.clear();
  }

  /**
   * Get registry stats
   */
  stats() {
    return {
      cachedStatements: this.statements.size,
      cachedParameterizations: this.parameterizedSqlCache.size,
    };
  }
}

/**
 * Helper to execute a prepared statement via EngineClient
 */
export async function executePreparedStatement(
  registry: PreparedStatementRegistry,
  client: EngineClient,
  statementId: string,
  parameters: (string | number | boolean | null)[]
): Promise<any> {
  const stmt = registry.getStatement(statementId);
  if (!stmt) {
    throw new Error(`Prepared statement not found: ${statementId}`);
  }

  // Bind parameters to get final SQL
  const finalSql = registry.bindParameters(stmt, parameters);

  // Execute via engine client
  return client.executeSql(finalSql);
}
