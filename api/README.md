# API Layer Guide

This folder contains the Phase 5 Week 25-26 API foundation for the database engine. The API is implemented with Express.js and forwards requests to the C++ engine binary.

## API Style

The current API uses HTTP + JSON and follows a REST-style resource model for the main database resources:

- `GET /tables`
- `POST /tables`
- `GET /tables/{tableName}`
- `GET /tables/{tableName}/rows`
- `POST /tables/{tableName}/rows`

Two endpoints are intentionally more command-oriented for this phase:

- `POST /init`
- `POST /sql`

Those two are pragmatic RPC-style endpoints used to bootstrap the database and execute raw SQL while the project is still in the API foundation phase. So the answer is: yes, this is a REST API in structure, with two deliberate non-resource endpoints for initialization and direct SQL execution.

## Prerequisites

1. Build the C++ engine:

```powershell
cmake --build ..\build --target dbcore_engine
```

2. Start the API server from the `api` folder:

```powershell
npm run dev
```

3. The default base URL is:

```text
http://localhost:3000
```

## Postman Testing Guide

Use Postman with this base URL:

```text
http://localhost:3000
```

Common headers for JSON endpoints:

```text
Content-Type: application/json
Accept: application/json
```

Recommended execution order in Postman:

1. `GET /health`
2. `POST /init`
3. `POST /tables`
4. `GET /tables`
5. `GET /tables/{tableName}`
6. `POST /tables/{tableName}/rows`
7. `GET /tables/{tableName}/rows`
8. `POST /sql`

### 1. Health Check

Method: `GET`

URL:

```text
http://localhost:3000/health
```

Query params: none

Body: none

Expected output:

```json
{
  "ok": true,
  "service": "advanced-database-api"
}
```

### 2. Initialize Database

Method: `POST`

URL:

```text
http://localhost:3000/init
```

Query params: none

Body: none

Expected output:

```json
{
  "ok": true
}
```

### 3. Create Table

Method: `POST`

URL:

```text
http://localhost:3000/tables
```

Query params: none

Body type: `raw` -> `JSON`

Body:

```json
{
  "name": "users",
  "columns": [
    { "name": "id", "type": "int" },
    { "name": "name", "type": "varchar", "length": 100 },
    { "name": "email", "type": "text", "nullable": true }
  ]
}
```

Expected output:

```json
{
  "ok": true,
  "table": "users"
}
```

Validation notes:
- `name` must be a non-empty string
- `columns` must contain at least one valid column definition
- allowed types are `int`, `text`, `varchar`
- if `length` is provided, it must be a positive integer

Example invalid request body:

```json
{
  "name": "users",
  "columns": []
}
```

Example error output:

```json
{
  "ok": false,
  "error": "Request body must include name and at least one valid column definition"
}
```

### 4. List Tables

Method: `GET`

URL:

```text
http://localhost:3000/tables
```

Query params: none

Body: none

Example output:

```json
{
  "ok": true,
  "tables": ["users"]
}
```

### 5. Describe Table

Method: `GET`

URL:

```text
http://localhost:3000/tables/users
```

Path params:
- `tableName = users`

Query params: none

Body: none

Example output:

```json
{
  "ok": true,
  "table": "users",
  "columns": [
    { "name": "id", "type": "int", "nullable": false },
    { "name": "name", "type": "varchar(100)", "nullable": false },
    { "name": "email", "type": "text", "nullable": true }
  ]
}
```

Note: the exact column payload depends on the current C++ engine JSON contract.

### 6. Insert Row

Method: `POST`

URL:

```text
http://localhost:3000/tables/users/rows
```

Path params:
- `tableName = users`

Query params: none

Body type: `raw` -> `JSON`

Body:

```json
{
  "values": {
    "id": 1,
    "name": "Ada",
    "email": "ada@example.com"
  }
}
```

Expected output:

```json
{
  "ok": true,
  "statement": "insert"
}
```

Example invalid request body:

```json
{
  "values": {}
}
```

Example error output:

```json
{
  "ok": false,
  "error": "Request body must include a non-empty values object"
}
```

### 7. Select Rows

Method: `GET`

URL without filters:

```text
http://localhost:3000/tables/users/rows
```

URL with filters:

```text
http://localhost:3000/tables/users/rows?column=id&op=gte&value=1
```

Path params:
- `tableName = users`

Optional query params:
- `column`
- `op` = `eq`, `neq`, `lt`, `lte`, `gt`, `gte`
- `value`

Body: none

Example output:

```json
{
  "ok": true,
  "statement": "select",
  "rows": [
    {
      "id": 1,
      "name": "Ada",
      "email": "ada@example.com"
    }
  ]
}
```

Example invalid params:
- `column=id` without `value`

Example error output:

```json
{
  "ok": false,
  "error": "Query parameters column and value are required together"
}
```

### 8. Execute SQL

Method: `POST`

URL:

```text
http://localhost:3000/sql
```

Query params: none

Body type: `raw` -> `JSON`

Body:

```json
{
  "sql": "SELECT * FROM users"
}
```

Example output:

```json
{
  "ok": true,
  "statement": "select",
  "rows": [
    {
      "id": 1,
      "name": "Ada",
      "email": "ada@example.com"
    }
  ]
}
```

Example invalid request body:

```json
{
  "sql": ""
}
```

Example error output:

```json
{
  "ok": false,
  "error": "Request body must include a non-empty sql string"
}
```

### 9. Invalid JSON Example

Method: `POST`

URL:

```text
http://localhost:3000/sql
```

Body type: `raw` -> `JSON`

Broken body:

```json
{"sql":
```

Expected output:

```json
{
  "ok": false,
  "error": "Invalid JSON body"
}
```

### 10. Unknown Route Example

Method: `GET`

URL:

```text
http://localhost:3000/missing
```

Expected output:

```json
{
  "ok": false,
  "error": "Route not found"
}
```

## Automated Tests

Run the endpoint test suite from the `api` folder:

```powershell
npm test
```

The Jest suite covers every Week 25-26 endpoint, invalid JSON handling, validation failures, and the 404 JSON contract.

## Postman Setup Notes

In Postman, create an environment with:

```text
baseUrl = http://localhost:3000
tableName = users
```

Then use URLs like:

```text
{{baseUrl}}/tables/{{tableName}}/rows
```

For the JSON endpoints, choose:
- Body -> `raw`
- Format -> `JSON`

For filtered row reads, use the Params tab:

```text
column = id
op = gte
value = 1
```