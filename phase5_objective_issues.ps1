param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "API Layer Development",
    [string]$OutFile = "phase5-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

# One issue per objective in each Phase 5 week section.
$issues = @(
    [pscustomobject]@{
        title = "P5 W25-26: Set up Express.js server"
        body  = "## Phase 5: API Layer Development`n## Section: Week 25-26 - Node.js API Foundation`n`n### Objective`nSet up Express.js server.`n`n### Acceptance Criteria`n- Express server starts and responds to HTTP requests`n- Server configuration is environment-aware (port, host)"
    }
    [pscustomobject]@{
        title = "P5 W25-26: Implement basic API endpoints"
        body  = "## Phase 5: API Layer Development`n## Section: Week 25-26 - Node.js API Foundation`n`n### Objective`nImplement basic API endpoints.`n`n### Acceptance Criteria`n- CRUD endpoints for database operations are exposed over HTTP`n- JSON request/response format is consistently applied"
    }
    [pscustomobject]@{
        title = "P5 W25-26: Add request validation and error handling"
        body  = "## Phase 5: API Layer Development`n## Section: Week 25-26 - Node.js API Foundation`n`n### Objective`nAdd request validation and error handling.`n`n### Acceptance Criteria`n- Invalid requests return structured error responses`n- Input validation middleware rejects malformed payloads"
    }
    [pscustomobject]@{
        title = "P5 W25-26: Establish C++/Node.js communication"
        body  = "## Phase 5: API Layer Development`n## Section: Week 25-26 - Node.js API Foundation`n`n### Objective`nEstablish C++/Node.js communication.`n`n### Acceptance Criteria`n- Node.js API layer can invoke database engine operations`n- Communication channel is reliable and handles errors gracefully"
    }
    [pscustomobject]@{
        title = "P5 W27-28: Implement authentication and authorization"
        body  = "## Phase 5: API Layer Development`n## Section: Week 27-28 - Advanced API Features`n`n### Objective`nImplement authentication and authorization.`n`n### Acceptance Criteria`n- JWT-based authentication is enforced on protected endpoints`n- Unauthorized requests are rejected with appropriate HTTP status codes"
    }
    [pscustomobject]@{
        title = "P5 W27-28: Add connection pooling"
        body  = "## Phase 5: API Layer Development`n## Section: Week 27-28 - Advanced API Features`n`n### Objective`nAdd connection pooling.`n`n### Acceptance Criteria`n- Connection pool manages lifecycle of database connections`n- Pool size and timeout are configurable"
    }
    [pscustomobject]@{
        title = "P5 W27-28: Support prepared statements"
        body  = "## Phase 5: API Layer Development`n## Section: Week 27-28 - Advanced API Features`n`n### Objective`nSupport prepared statements.`n`n### Acceptance Criteria`n- API supports parameterized queries to prevent SQL injection`n- Prepared statement execution is validated with tests"
    }
    [pscustomobject]@{
        title = "P5 W27-28: Add API documentation"
        body  = "## Phase 5: API Layer Development`n## Section: Week 27-28 - Advanced API Features`n`n### Objective`nAdd API documentation.`n`n### Acceptance Criteria`n- OpenAPI/Swagger documentation describes all endpoints`n- Documentation is accessible via a served endpoint"
    }
)

function Get-EncodedIssueUrl {
    param(
        [string]$BaseRepoUrl,
        [string]$Title,
        [string]$Body,
        [string]$Milestone,
        [string[]]$Labels
    )

    $titleParam = [uri]::EscapeDataString($Title)
    $bodyParam = [uri]::EscapeDataString($Body)
    $milestoneParam = [uri]::EscapeDataString($Milestone)
    $labelsParam = [uri]::EscapeDataString(($Labels -join ","))

    "$BaseRepoUrl/issues/new?title=$titleParam&body=$bodyParam&milestone=$milestoneParam&labels=$labelsParam"
}

$labels = @("phase-5", "api-layer")

$ghAvailable = $null -ne (Get-Command gh -ErrorAction SilentlyContinue)
$ghAuthenticated = $false

if ($ghAvailable) {
    try {
        gh auth status | Out-Null
        $ghAuthenticated = $true
    } catch {
        $ghAuthenticated = $false
    }
}

if ($ghAuthenticated) {
    Write-Host "GitHub CLI is available and authenticated. Creating issues directly in $repoSlug..." -ForegroundColor Green

    $milestones = gh api "repos/$repoSlug/milestones?state=all" | ConvertFrom-Json
    $milestone = $milestones | Where-Object { $_.title -eq $MilestoneTitle } | Select-Object -First 1

    if (-not $milestone) {
        Write-Host "Milestone '$MilestoneTitle' does not exist. Creating it..." -ForegroundColor Yellow
        $milestone = gh api "repos/$repoSlug/milestones" --method POST --field "title=$MilestoneTitle" | ConvertFrom-Json
    }

    foreach ($issue in $issues) {
        $bodyWithMilestoneNote = "$($issue.body)`n`nMilestone: $MilestoneTitle"

        gh issue create `
            --repo $repoSlug `
            --title $issue.title `
            --body $bodyWithMilestoneNote `
            --milestone $MilestoneTitle `
            --label $labels[0] `
            --label $labels[1] | Out-Null

        Write-Host "Created: $($issue.title)" -ForegroundColor Cyan
    }

    Write-Host "Created $($issues.Count) Phase 5 issues under milestone '$MilestoneTitle'." -ForegroundColor Green
} else {
    Write-Warning "GitHub CLI is not available/authenticated. Falling back to prefilled issue links."
    Write-Warning "Milestone is included in URL as '$MilestoneTitle'. If GitHub ignores it, set milestone manually in the issue form."

    $links = $issues | ForEach-Object {
        Get-EncodedIssueUrl -BaseRepoUrl $repoUrl -Title $_.title -Body $_.body -Milestone $MilestoneTitle -Labels $labels
    }

    $links | Set-Content -Path $OutFile
    Write-Host "Saved $($links.Count) links to '$OutFile'." -ForegroundColor Green

    if ($OpenInBrowser) {
        foreach ($link in $links) {
            Start-Process $link
            Start-Sleep -Milliseconds 250
        }
    }
}
