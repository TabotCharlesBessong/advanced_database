param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "Query processing",
    [string]$OutFile = "phase3-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

# One issue per objective in each Phase 3 week section.
$issues = @(
    [pscustomobject]@{
        title = "P3 W13-14: Implement query execution framework"
        body  = "## Phase 3: Query Processing`n## Section: Week 13-14 - Query Execution Engine`n`n### Objective`nImplement query execution framework.`n`n### Acceptance Criteria`n- Execution framework composes operators correctly`n- End-to-end execution tests pass for representative queries"
    }
    [pscustomobject]@{
        title = "P3 W13-14: Create iterator-based execution model"
        body  = "## Phase 3: Query Processing`n## Section: Week 13-14 - Query Execution Engine`n`n### Objective`nCreate iterator-based execution model.`n`n### Acceptance Criteria`n- Volcano-style operator lifecycle is implemented (open/next/close)`n- Iterator pipeline tests pass"
    }
    [pscustomobject]@{
        title = "P3 W13-14: Support projection and filtering operations"
        body  = "## Phase 3: Query Processing`n## Section: Week 13-14 - Query Execution Engine`n`n### Objective`nSupport projection and filtering operations.`n`n### Acceptance Criteria`n- SELECT projection returns expected columns and order`n- WHERE filtering returns correct result subsets"
    }
    [pscustomobject]@{
        title = "P3 W13-14: Build execution plan representation"
        body  = "## Phase 3: Query Processing`n## Section: Week 13-14 - Query Execution Engine`n`n### Objective`nBuild execution plan representation.`n`n### Acceptance Criteria`n- Plan tree captures scan/filter/project structure`n- Plan output can be serialized for inspection"
    }
    [pscustomobject]@{
        title = "P3 W15-16: Implement JOIN operations"
        body  = "## Phase 3: Query Processing`n## Section: Week 15-16 - Advanced SQL Features`n`n### Objective`nImplement JOIN operations.`n`n### Acceptance Criteria`n- At least one join algorithm (nested loop) is available`n- Join correctness tests pass for inner joins"
    }
    [pscustomobject]@{
        title = "P3 W15-16: Support aggregate functions (COUNT, SUM, AVG)"
        body  = "## Phase 3: Query Processing`n## Section: Week 15-16 - Advanced SQL Features`n`n### Objective`nSupport aggregate functions (COUNT, SUM, AVG).`n`n### Acceptance Criteria`n- Aggregates return correct values for grouped and ungrouped input`n- Aggregate edge cases (empty input, null handling strategy) are tested"
    }
    [pscustomobject]@{
        title = "P3 W15-16: Add GROUP BY and HAVING clauses"
        body  = "## Phase 3: Query Processing`n## Section: Week 15-16 - Advanced SQL Features`n`n### Objective`nAdd GROUP BY and HAVING clauses.`n`n### Acceptance Criteria`n- Grouping partitions rows correctly`n- HAVING filters groups using aggregate predicates"
    }
    [pscustomobject]@{
        title = "P3 W15-16: Implement ORDER BY sorting"
        body  = "## Phase 3: Query Processing`n## Section: Week 15-16 - Advanced SQL Features`n`n### Objective`nImplement ORDER BY sorting.`n`n### Acceptance Criteria`n- ORDER BY sorts on one and multiple keys`n- ASC and DESC behavior is validated by tests"
    }
    [pscustomobject]@{
        title = "P3 W17-18: Implement basic query planner"
        body  = "## Phase 3: Query Processing`n## Section: Week 17-18 - Query Optimization`n`n### Objective`nImplement basic query planner.`n`n### Acceptance Criteria`n- Planner produces executable plans for supported query forms`n- Plan generation tests pass"
    }
    [pscustomobject]@{
        title = "P3 W17-18: Add cost-based optimization"
        body  = "## Phase 3: Query Processing`n## Section: Week 17-18 - Query Optimization`n`n### Objective`nAdd cost-based optimization.`n`n### Acceptance Criteria`n- Cost model compares candidate plans`n- Optimizer picks lower-cost plans in benchmarked cases"
    }
    [pscustomobject]@{
        title = "P3 W17-18: Create statistics collection"
        body  = "## Phase 3: Query Processing`n## Section: Week 17-18 - Query Optimization`n`n### Objective`nCreate statistics collection.`n`n### Acceptance Criteria`n- Table/column stats needed by optimizer are collected`n- Stats refresh/update behavior is documented and tested"
    }
    [pscustomobject]@{
        title = "P3 W17-18: Support query plan visualization"
        body  = "## Phase 3: Query Processing`n## Section: Week 17-18 - Query Optimization`n`n### Objective`nSupport query plan visualization.`n`n### Acceptance Criteria`n- Plan explain output is human-readable and stable`n- Visualization output is covered by regression tests"
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

$labels = @("phase-3", "query-processing")

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

    Write-Host "Created $($issues.Count) Phase 3 issues under milestone '$MilestoneTitle'." -ForegroundColor Green
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
