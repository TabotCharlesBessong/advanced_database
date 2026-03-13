# Advanced Database API

REST API for interacting with a high-performance C++ database engine. Features include transaction management, concurrency control, JWT authentication, connection pooling, prepared statements, and full OpenAPI documentation.

## Quick Start

### Prerequisites

- Node.js 18+ installed
- npm installed
- The C++ database engine running (or available at engine binary path)

### Installation

```bash
cd api
npm install
npm run build
npm start
```

The API will be available at `http://localhost:3000`

### API Documentation

**Interactive Documentation**: Visit `http://localhost:3000/api-docs` to explore the API with Swagger UI.

## Authentication

### JWT Bearer Tokens

All endpoints except `/health` and `/auth/login` require authentication via JWT bearer tokens.

#### Obtain a Token

```http
POST /auth/login
Content-Type: application/json

{
  "username": "admin",
  "password": "password123"
}
```

Response:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expiresIn": "24h",
  "user": { "username": "admin" }
}
```

#### Use Token in Requests

Add the token to the `Authorization` header of subsequent requests:

```http
GET /tables
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

In Postman:
1. Copy the token from login response
2. Go to the request's **Auth** tab
3. Select "Bearer Token" type
4. Paste the token in the **Token** field

## Connection Pooling

The API uses an intelligent connection pool to manage multiple database connections:

- **Min connections**: 2 (always available)
- **Max connections**: 10 (scales up under load)
- **Idle timeout**: 30 seconds (unused connections are recycled)

### Check Pool Status

```http
GET /stats/pool
Authorization: Bearer <token>
```

Response:
```json
{
  "totalConnections": 5,
  "inUse": 2,
  "idle": 3,
  "waitingRequests": 0,
  "poolSize": { "min": 2, "max": 10 }
}
```

## Prepared Statements

Prepared statements prevent SQL injection and improve performance for repeated queries.

### Step 1: Prepare a Statement

```http
POST /prepare
Authorization: Bearer <token>
Content-Type: application/json

{
  "sql": "SELECT * FROM users WHERE id = ? AND active = ?"
}
```

Response:
```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "sql": "SELECT * FROM users WHERE id = ? AND active = ?",
  "paramCount": 2,
  "paramTypes": ["unknown", "unknown"]
}
```

### Step 2: Execute with Parameters

```http
POST /execute
Authorization: Bearer <token>
Content-Type: application/json

{
  "statementId": "550e8400-e29b-41d4-a716-446655440000",
  "parameters": [1, true]
}
```

Response:
```json
[
  { "id": 1, "name": "Ada", "active": true }
]
```

**Parameter Safety**: Parameters are properly escaped to prevent SQL injection. Type coercion is automatic:
- Numbers passed as-is
- Strings escaped and quoted
- Booleans converted to 1/0
- null becomes NULL

## REST API Endpoints

### System Endpoints

#### GET /health
Check API and database engine health.

```http
GET /health
```

Response (200 OK):
```json
{
  "ok": true,
  "service": "advanced-database-api",
  "timestamp": "2025-01-01T12:00:00.000Z"
}
```

### Table Management

#### POST /tables
Create a new table.

```http
POST /tables
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "users",
  "columns": [
    { "name": "id", "type": "INT" },
    { "name": "name", "type": "TEXT" },
    { "name": "email", "type": "TEXT" }
  ]
}
```

Response (200 OK):
```json
{ "ok": true }
```

#### GET /tables
List all tables in the database.

```http
GET /tables
Authorization: Bearer <token>
```

Response (200 OK):
```json
[
  {
    "name": "users",
    "columns": [
      { "name": "id", "type": "INT" },
      { "name": "name", "type": "TEXT" }
    ],
    "rowCount": 42
  }
]
```

#### GET /tables/:tableName
Describe a specific table's schema.

```http
GET /tables/users
Authorization: Bearer <token>
```

Response (200 OK):
```json
{
  "name": "users",
  "columns": [
    { "name": "id", "type": "INT" },
    { "name": "name", "type": "TEXT" },
    { "name": "email", "type": "TEXT" }
  ],
  "rowCount": 42
}
```

### Row Operations

#### GET /tables/:tableName/rows
Select rows from a table with optional filtering.

```http
GET /tables/users/rows
Authorization: Bearer <token>
```

Query parameters:
- `condition` (optional): WHERE clause condition, e.g., "id > 5"

Example with filter:
```http
GET /tables/users/rows?condition=id%20%3E%205
```

Response (200 OK):
```json
[
  { "id": 6, "name": "Bob", "email": "bob@example.com" },
  { "id": 7, "name": "Carol", "email": "carol@example.com" }
]
```

#### POST /tables/:tableName/rows
Insert a new row into a table.

```http
POST /tables/users/rows
Authorization: Bearer <token>
Content-Type: application/json

{
  "id": 43,
  "name": "David",
  "email": "david@example.com"
}
```

Response (200 OK):
```json
{ "ok": true }
```

### Advanced Query

#### POST /sql
Execute raw SQL commands (for advanced queries not covered by REST endpoints).

```http
POST /sql
Authorization: Bearer <token>
Content-Type: application/json

{
  "sql": "SELECT COUNT(*) as count FROM users WHERE active = 1"
}
```

Response (200 OK):
```json
[
  { "count": 35 }
]
```

Warning: Avoid using this endpoint for user input without proper validation. Use **prepared statements** for parameterized queries instead.

#### POST /init
Initialize the database engine for the current session.

```http
POST /init
Authorization: Bearer <token>
```

Response (200 OK):
```json
{ "ok": true }
```

## Testing with Postman

### Recommended Test Sequence

1. **Login** (POST /auth/login)
   - Obtain JWT token
   - Save token for subsequent requests

2. **Check Health** (GET /health)
   - Verify API is running
   - *No auth required*

3. **Initialize Database** (POST /init)
   - Set up database engine for session
   - *Auth required*

4. **Create Table** (POST /tables)
   - Define schema: `users` table
   - Columns: id (INT), name (TEXT), email (TEXT)
   - *Auth required*

5. **List Tables** (GET /tables)
   - Verify table creation
   - *Auth required*

6. **Describe Table** (GET /tables/users)
   - Check column definitions
   - *Auth required*

7. **Insert Rows** (POST /tables/users/rows)
   - Add test data
   - Insert 3-5 rows for testing
   - *Auth required*

8. **Select Rows** (GET /tables/users/rows)
   - Retrieve all rows
   - Try with `condition` filter
   - *Auth required*

9. **Prepared Statement** (POST /prepare)
   - Prepare: "SELECT * FROM users WHERE id = ?"
   - Save returned statement ID
   - *Auth required*

10. **Execute Prepared** (POST /execute)
    - Execute with different parameter values
    - Verify SQL injection prevention
    - *Auth required*

11. **Pool Stats** (GET /stats/pool)
    - Monitor connection pool
    - *Auth required*

12. **Raw SQL** (POST /sql)
    - Execute COUNT(*) query
    - *Auth required*

### Postman Environment Setup

Create a Postman environment with these variables:

| Variable | Value | Type |
|----------|-------|------|
| `baseUrl` | `http://localhost:3000` | string |
| `token` | *(obtained from login)* | string |
| `tableName` | `users` | string |

Then use `{{baseUrl}}`, `{{token}}`, `{{tableName}}` in request URLs and headers.

## REST API Style

This API follows REST principles for **resource operations** (tables, rows) while including pragmatic **command endpoints** for initialization and raw SQL:

**REST Endpoints** (resource-oriented):
- `GET /tables` - List collections
- `POST /tables` - Create resource
- `GET /tables/:id` - Retrieve specific resource
- `GET /tables/:id/rows` - Retrieve sub-resources
- `POST /tables/:id/rows` - Create sub-resource

**Command Endpoints** (RPC-style, for MVP):
- `POST /init` - Initialize session
- `POST /sql` - Execute arbitrary SQL
- `POST /prepare` - Prepare statement
- `POST /execute` - Execute prepared statement

This hybrid approach balances REST principles with practical database flexibility.

## Error Handling

All endpoints return JSON error responses:

```json
{
  "error": "Descriptive error message"
}
```

**Status Codes**:
- `200 OK` - Request succeeded
- `400 Bad Request` - Invalid input (validation error)
- `401 Unauthorized` - Missing or invalid authentication token
- `404 Not Found` - Resource or endpoint not found
- `500 Internal Server Error` - Server error

## Security

### Production Deployment

⚠️ **Important**: The default dev mode accepts any username/password for `/auth/login`. For production:

1. Implement proper password hashing (bcrypt)
2. Store credentials in a secure database
3. Set a strong `JWT_SECRET` environment variable
4. Use HTTPS for all traffic
5. Add rate limiting to `/auth/login`
6. Consider adding request signing for audit trails

Environment variables:
```bash
JWT_SECRET=your-secret-key-min-32-chars
JWT_EXPIRY=24h
NODE_ENV=production
```

## Development

### Scripts

```bash
npm run build     # Compile TypeScript
npm test          # Run Jest tests
npm start         # Start production server
npm run dev       # Start with nodemon (auto-reload)
```

### Project Structure

```
api/
├── app.ts                    # Express app factory with routes
├── auth.ts                   # JWT authentication middleware
├── engineClient.ts           # C++ engine binary bridge
├── pool.ts                   # Connection pool manager
├── prepared-statements.ts    # Prepared statement registry
├── swagger-config.ts         # OpenAPI spec generation
├── validation.ts             # Request validation guards
├── index.ts                  # Server entry point
├── Database.test.ts          # Jest test suite
├── package.json
├── tsconfig.json
└── README.md                 # This file
```

## Monitoring

### Pool Statistics

The `/stats/pool` endpoint provides real-time connection pool metrics:

```json
{
  "totalConnections": 3,
  "inUse": 1,
  "idle": 2,
  "waitingRequests": 0,
  "poolSize": { "min": 2, "max": 10 }
}
```

Use these metrics to detect connection bottlenecks or pool exhaustion.

## Support

For issues or questions:
1. Check the Swagger UI at `/api-docs` for detailed endpoint specs
2. Review test cases in `Database.test.ts` for example usage
3. Check server logs for error details
