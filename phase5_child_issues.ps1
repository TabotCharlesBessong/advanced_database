$repo='https://github.com/TabotCharlesBessong/advanced_database'
$issues=@(
[pscustomobject]@{title='P5 W25-26: Set up Express.js server'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 25-26: Node.js API Foundation`n`n## Objective`nSet up Express.js server.`n`n## Acceptance Criteria`n- Express server starts and responds to HTTP requests`n- Server configuration is environment-aware (port, host)'}
[pscustomobject]@{title='P5 W25-26: Implement basic API endpoints'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 25-26: Node.js API Foundation`n`n## Objective`nImplement basic API endpoints.`n`n## Acceptance Criteria`n- CRUD endpoints for database operations are exposed over HTTP`n- JSON request/response format is consistently applied'}
[pscustomobject]@{title='P5 W25-26: Add request validation and error handling'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 25-26: Node.js API Foundation`n`n## Objective`nAdd request validation and error handling.`n`n## Acceptance Criteria`n- Invalid requests return structured error responses`n- Input validation middleware rejects malformed payloads'}
[pscustomobject]@{title='P5 W25-26: Establish C++/Node.js communication'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 25-26: Node.js API Foundation`n`n## Objective`nEstablish C++/Node.js communication.`n`n## Acceptance Criteria`n- Node.js API layer can invoke database engine operations`n- Communication channel is reliable and handles errors gracefully'}
[pscustomobject]@{title='P5 W27-28: Implement authentication and authorization'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 27-28: Advanced API Features`n`n## Objective`nImplement authentication and authorization.`n`n## Acceptance Criteria`n- JWT-based authentication is enforced on protected endpoints`n- Unauthorized requests are rejected with appropriate HTTP status codes'}
[pscustomobject]@{title='P5 W27-28: Add connection pooling'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 27-28: Advanced API Features`n`n## Objective`nAdd connection pooling.`n`n## Acceptance Criteria`n- Connection pool manages lifecycle of database connections`n- Pool size and timeout are configurable'}
[pscustomobject]@{title='P5 W27-28: Support prepared statements'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 27-28: Advanced API Features`n`n## Objective`nSupport prepared statements.`n`n## Acceptance Criteria`n- API supports parameterized queries to prevent SQL injection`n- Prepared statement execution is validated with tests'}
[pscustomobject]@{title='P5 W27-28: Add API documentation'; body='## Parent Epic`nPhase 5 (Weeks 25-28): API Layer Development`n`n## Section`nWeek 27-28: Advanced API Features`n`n## Objective`nAdd API documentation.`n`n## Acceptance Criteria`n- OpenAPI/Swagger documentation describes all endpoints`n- Documentation is accessible via a served endpoint'}
)

$links = $issues | ForEach-Object {
  $title=[uri]::EscapeDataString($_.title)
  $body=[uri]::EscapeDataString($_.body)
  "$repo/issues/new?title=$title&body=$body"
}
$links | Set-Content -Path 'phase5-child-issue-links.txt'
$links
