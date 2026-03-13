import swaggerJsdoc from 'swagger-jsdoc';

/**
 * Swagger/OpenAPI 3.0 configuration for API documentation
 */
export function getSwaggerDefinition() {
  const definition = {
    definition: {
      openapi: '3.0.0',
      info: {
        title: 'Advanced Database API',
        version: '0.4.0',
        description:
          'REST API for interacting with a high-performance C++ database engine. ' +
          'Features include transaction management, concurrency control, and prepared statements.',
        contact: {
          name: 'Development Team',
          url: 'https://github.com/yourrepo',
        },
      },
      servers: [
        {
          url: process.env.API_SERVER_URL || 'http://localhost:3000',
          description:
            process.env.NODE_ENV === 'production'
              ? 'Production server'
              : 'Development server',
        },
      ],
      components: {
        securitySchemes: {
          bearerAuth: {
            type: 'http',
            scheme: 'bearer',
            bearerFormat: 'JWT',
            description: 'JWT Bearer token. Obtain via POST /auth/login',
          },
        },
        schemas: {
          HealthResponse: {
            type: 'object',
            properties: {
              status: { type: 'string', example: 'ok' },
              uptime: { type: 'number', example: 1234.56 },
            },
          },
          Error: {
            type: 'object',
            properties: {
              error: { type: 'string' },
            },
          },
          Table: {
            type: 'object',
            properties: {
              name: { type: 'string', example: 'users' },
              columns: {
                type: 'array',
                items: {
                  type: 'object',
                  properties: {
                    name: { type: 'string' },
                    type: { type: 'string', enum: ['INT', 'TEXT', 'REAL'] },
                  },
                },
              },
              rowCount: { type: 'number', example: 42 },
            },
          },
          Row: {
            type: 'object',
            additionalProperties: {
              oneOf: [
                { type: 'string' },
                { type: 'number' },
                { type: 'boolean' },
                { type: 'null' },
              ],
            },
            example: { id: 1, name: 'John', active: true },
          },
          JwtToken: {
            type: 'object',
            properties: {
              token: {
                type: 'string',
                example:
                  'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...',
              },
              expiresIn: { type: 'string', example: '24h' },
            },
          },
          PreparedStatement: {
            type: 'object',
            properties: {
              id: { type: 'string', format: 'uuid' },
              sql: { type: 'string', example: 'SELECT * FROM users WHERE id = ?' },
              paramCount: { type: 'number', example: 1 },
              paramTypes: { type: 'array', items: { type: 'string' } },
            },
          },
          PoolStats: {
            type: 'object',
            properties: {
              totalConnections: { type: 'number' },
              inUse: { type: 'number' },
              idle: { type: 'number' },
              waitingRequests: { type: 'number' },
              poolSize: {
                type: 'object',
                properties: {
                  min: { type: 'number' },
                  max: { type: 'number' },
                },
              },
            },
          },
        },
      },
      paths: {
        '/health': {
          get: {
            tags: ['System'],
            summary: 'Health check endpoint',
            description: 'Check if the API server and database engine are running',
            responses: {
              200: {
                description: 'Server is healthy',
                content: {
                  'application/json': {
                    schema: {
                      $ref: '#/components/schemas/HealthResponse',
                    },
                  },
                },
              },
            },
          },
        },

        '/auth/login': {
          post: {
            tags: ['Authentication'],
            summary: 'Obtain JWT token',
            description:
              'Login with username/password to get a JWT bearer token for subsequent requests',
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: {
                    type: 'object',
                    properties: {
                      username: {
                        type: 'string',
                        example: 'admin',
                      },
                      password: {
                        type: 'string',
                        example: 'password123',
                      },
                    },
                    required: ['username', 'password'],
                  },
                },
              },
            },
            responses: {
              200: {
                description: 'Login successful',
                content: {
                  'application/json': {
                    schema: {
                      $ref: '#/components/schemas/JwtToken',
                    },
                  },
                },
              },
              401: {
                description: 'Invalid credentials',
                content: {
                  'application/json': {
                    schema: { $ref: '#/components/schemas/Error' },
                  },
                },
              },
            },
          },
        },

        '/init': {
          post: {
            tags: ['Database'],
            summary: 'Initialize database',
            description:
              'Initialize the database engine for the current session',
            security: [{ bearerAuth: [] }],
            responses: {
              200: {
                description: 'Database initialized',
                content: {
                  'application/json': {
                    schema: {
                      type: 'object',
                      properties: {
                        message: { type: 'string' },
                      },
                    },
                  },
                },
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/tables': {
          post: {
            tags: ['Tables'],
            summary: 'Create a new table',
            description: 'Create a table with specified columns and types',
            security: [{ bearerAuth: [] }],
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: {
                    type: 'object',
                    properties: {
                      name: { type: 'string', example: 'users' },
                      columns: {
                        type: 'array',
                        items: {
                          type: 'object',
                          properties: {
                            name: { type: 'string' },
                            type: {
                              type: 'string',
                              enum: ['INT', 'TEXT', 'REAL'],
                            },
                          },
                        },
                      },
                    },
                    required: ['name', 'columns'],
                  },
                },
              },
            },
            responses: {
              200: {
                description: 'Table created',
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
          get: {
            tags: ['Tables'],
            summary: 'List all tables',
            description: 'Get list of all tables in the database',
            security: [{ bearerAuth: [] }],
            responses: {
              200: {
                description: 'List of tables',
                content: {
                  'application/json': {
                    schema: {
                      type: 'array',
                      items: { $ref: '#/components/schemas/Table' },
                    },
                  },
                },
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/tables/{tableName}': {
          get: {
            tags: ['Tables'],
            summary: 'Describe table',
            description: 'Get schema and metadata for a specific table',
            security: [{ bearerAuth: [] }],
            parameters: [
              {
                name: 'tableName',
                in: 'path',
                required: true,
                schema: { type: 'string' },
              },
            ],
            responses: {
              200: {
                description: 'Table schema',
                content: {
                  'application/json': {
                    schema: { $ref: '#/components/schemas/Table' },
                  },
                },
              },
              404: {
                description: 'Table not found',
              },
            },
          },
        },

        '/tables/{tableName}/rows': {
          get: {
            tags: ['Rows'],
            summary: 'Select rows',
            description:
              'Query rows from a table with optional filtering and parameters',
            security: [{ bearerAuth: [] }],
            parameters: [
              {
                name: 'tableName',
                in: 'path',
                required: true,
                schema: { type: 'string' },
              },
              {
                name: 'condition',
                in: 'query',
                schema: { type: 'string' },
                description: 'WHERE clause condition (e.g., "id = 1")',
              },
            ],
            responses: {
              200: {
                description: 'List of rows',
                content: {
                  'application/json': {
                    schema: {
                      type: 'array',
                      items: { $ref: '#/components/schemas/Row' },
                    },
                  },
                },
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
          post: {
            tags: ['Rows'],
            summary: 'Insert row',
            description: 'Insert a new row into a table',
            security: [{ bearerAuth: [] }],
            parameters: [
              {
                name: 'tableName',
                in: 'path',
                required: true,
                schema: { type: 'string' },
              },
            ],
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: { $ref: '#/components/schemas/Row' },
                },
              },
            },
            responses: {
              200: {
                description: 'Row inserted',
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/sql': {
          post: {
            tags: ['Query'],
            summary: 'Execute raw SQL',
            description:
              'Execute raw SQL command. Use for advanced queries not covered by REST endpoints.',
            security: [{ bearerAuth: [] }],
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: {
                    type: 'object',
                    properties: {
                      sql: {
                        type: 'string',
                        example: 'SELECT COUNT(*) as count FROM users',
                      },
                    },
                    required: ['sql'],
                  },
                },
              },
            },
            responses: {
              200: {
                description: 'SQL executed',
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/prepare': {
          post: {
            tags: ['Prepared Statements'],
            summary: 'Prepare a statement',
            description:
              'Pre-parse a SQL statement with ? placeholders for later execution with parameters',
            security: [{ bearerAuth: [] }],
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: {
                    type: 'object',
                    properties: {
                      sql: {
                        type: 'string',
                        example: 'SELECT * FROM users WHERE id = ?',
                      },
                    },
                    required: ['sql'],
                  },
                },
              },
            },
            responses: {
              200: {
                description: 'Statement prepared',
                content: {
                  'application/json': {
                    schema: {
                      $ref: '#/components/schemas/PreparedStatement',
                    },
                  },
                },
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/execute': {
          post: {
            tags: ['Prepared Statements'],
            summary: 'Execute prepared statement',
            description:
              'Execute a previously prepared statement by ID with parameters',
            security: [{ bearerAuth: [] }],
            requestBody: {
              required: true,
              content: {
                'application/json': {
                  schema: {
                    type: 'object',
                    properties: {
                      statementId: {
                        type: 'string',
                        format: 'uuid',
                      },
                      parameters: {
                        type: 'array',
                        items: {
                          oneOf: [
                            { type: 'string' },
                            { type: 'number' },
                            { type: 'boolean' },
                            { type: 'null' },
                          ],
                        },
                      },
                    },
                    required: ['statementId', 'parameters'],
                  },
                },
              },
            },
            responses: {
              200: {
                description: 'Statement executed',
              },
              401: {
                description: 'Unauthorized',
              },
              404: {
                description: 'Statement not found',
              },
            },
          },
        },

        '/stats/pool': {
          get: {
            tags: ['Monitoring'],
            summary: 'Connection pool statistics',
            description:
              'Get current connection pool usage and capacity statistics',
            security: [{ bearerAuth: [] }],
            responses: {
              200: {
                description: 'Pool stats',
                content: {
                  'application/json': {
                    schema: {
                      $ref: '#/components/schemas/PoolStats',
                    },
                  },
                },
              },
              401: {
                description: 'Unauthorized',
              },
            },
          },
        },

        '/api-docs': {
          get: {
            tags: ['Documentation'],
            summary: 'API documentation (Swagger UI)',
            description: 'Interactive API documentation powered by Swagger UI',
            responses: {
              200: {
                description: 'Swagger UI HTML',
              },
            },
          },
        },
      },
    },
    apis: [], // Can add JSDoc comment parsing here later
    };
    return definition;
}

/**
 * Generate the OpenAPI spec document
 */
export function getSwaggerSpec() {
  return swaggerJsdoc(getSwaggerDefinition());
}
