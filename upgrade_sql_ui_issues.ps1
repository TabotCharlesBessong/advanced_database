param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "SQL Compatibility and Product Hardening",
    [string]$OutFile = "upgrade-sql-ui-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

$issues = @(
    [pscustomobject]@{
        title = "Upgrade: SQL compatibility normalization for CREATE TABLE constraints"
        body  = "## Context`nRecent SQL scripts include PostgreSQL-style constraints and types not fully supported by the current engine parser.`n`n### Scope`n- Normalize unsupported CREATE TABLE tokens in API compatibility layer`n- Return structured warnings indicating rewritten syntax`n`n### Acceptance Criteria`n- Scripts containing PRIMARY KEY, UNIQUE, CHECK, DATE do not fail at parser stage when otherwise valid`n- API response includes explicit compatibility warnings`n- Automated tests cover normalization behavior"
    }
    [pscustomobject]@{
        title = "Upgrade: Support INSERT column-list and multi-row VALUES scripts"
        body  = "## Context`nBulk INSERT scripts currently fail because the engine expects a narrower INSERT format.`n`n### Scope`n- Expand INSERT INTO <table>(col...) VALUES (...), (...) into engine-compatible statements`n- Preserve clear failure reporting on partial insert failure`n`n### Acceptance Criteria`n- Multi-row INSERT with column list executes successfully via API endpoint`n- Failure response includes failed statement index when an insert step fails`n- Test suite covers expansion logic"
    }
    [pscustomobject]@{
        title = "Upgrade: Add database catalog workflow (CREATE DATABASE, USE, list databases)"
        body  = "## Context`nUsers requested database-level workflows beyond a single fixed data file.`n`n### Scope`n- Support CREATE DATABASE and USE commands at API level`n- Add endpoint to list available databases`n- Persist per-user selected database context for subsequent operations`n`n### Acceptance Criteria`n- CREATE DATABASE returns success and selects the created database`n- USE switches context and affects table/query operations`n- GET /databases returns known databases and current selection"
    }
    [pscustomobject]@{
        title = "Upgrade: ER diagram service and UI integration per selected database"
        body  = "## Context`nUsers need schema relationship visibility for each database context.`n`n### Scope`n- Add API endpoint returning ER model (tables + inferred relations)`n- Render ER diagram panel in UI Data Browser`n- Refresh ER view after schema-changing operations`n`n### Acceptance Criteria`n- ER endpoint returns stable table metadata for current database`n- UI renders table cards and relation list without blocking core query flow`n- Browser view updates ER data after CREATE TABLE/USE/CREATE DATABASE"
    }
    [pscustomobject]@{
        title = "Upgrade: SQL editor usability improvements (run selection + responsive workspace)"
        body  = "## Context`nEditor workflow needs better script control and layout room for real-world SQL authoring.`n`n### Scope`n- Add Run Selection action and keyboard shortcut support`n- Improve responsive layout and increase SQL editor workspace`n- Keep side panels accessible without shrinking editor excessively`n`n### Acceptance Criteria`n- Selected SQL can be executed independently from full script`n- Ctrl+Enter/Cmd+Enter triggers selection-first execution`n- Desktop and tablet layouts provide significantly more editor space"
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

$labels = @("enhancement", "phase-9")

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

    Write-Host "Created $($issues.Count) upgrade issues under milestone '$MilestoneTitle'." -ForegroundColor Green
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
