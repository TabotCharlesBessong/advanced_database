param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "Indexing and performance",
    [string]$OutFile = "phase4-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

# One issue per objective in each Phase 4 week section.
$issues = @(
    [pscustomobject]@{
        title = "P4 W19-20: Implement B-Tree data structure"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 19-20 - B-Tree Index Implementation`n`n### Objective`nImplement B-Tree data structure.`n`n### Acceptance Criteria`n- B-Tree supports insert/search operations for indexed keys`n- Core B-Tree behavior is validated with unit tests"
    }
    [pscustomobject]@{
        title = "P4 W19-20: Create index creation and maintenance"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 19-20 - B-Tree Index Implementation`n`n### Objective`nCreate index creation and maintenance.`n`n### Acceptance Criteria`n- Indexes can be created on supported column types`n- Index entries are maintained on row insert/update/delete paths"
    }
    [pscustomobject]@{
        title = "P4 W19-20: Support point queries and range scans"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 19-20 - B-Tree Index Implementation`n`n### Objective`nSupport point queries and range scans.`n`n### Acceptance Criteria`n- Equality predicates can be served by index lookup`n- Range predicates (>, >=, <, <=) can be served by index scan"
    }
    [pscustomobject]@{
        title = "P4 W19-20: Integrate indexes with query execution"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 19-20 - B-Tree Index Implementation`n`n### Objective`nIntegrate indexes with query execution.`n`n### Acceptance Criteria`n- Planner/executor can choose index access path when beneficial`n- Query correctness remains consistent with table-scan execution"
    }
    [pscustomobject]@{
        title = "P4 W21-22: Implement basic transaction support"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 21-22 - Transaction Management`n`n### Objective`nImplement basic transaction support.`n`n### Acceptance Criteria`n- BEGIN/COMMIT/ROLLBACK lifecycle is supported`n- Multi-statement transactions preserve atomicity for supported operations"
    }
    [pscustomobject]@{
        title = "P4 W21-22: Add Write-Ahead Logging (WAL)"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 21-22 - Transaction Management`n`n### Objective`nAdd Write-Ahead Logging (WAL).`n`n### Acceptance Criteria`n- WAL records changes before data pages are persisted`n- Recovery can replay/undo changes from WAL"
    }
    [pscustomobject]@{
        title = "P4 W21-22: Support BEGIN, COMMIT, ROLLBACK"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 21-22 - Transaction Management`n`n### Objective`nSupport BEGIN, COMMIT, ROLLBACK.`n`n### Acceptance Criteria`n- SQL parser and engine accept transaction commands`n- Transaction state transitions are validated by tests"
    }
    [pscustomobject]@{
        title = "P4 W21-22: Ensure atomicity and durability"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 21-22 - Transaction Management`n`n### Objective`nEnsure atomicity and durability.`n`n### Acceptance Criteria`n- Crash/restart test demonstrates committed data durability`n- Partially applied transactions are not visible after recovery"
    }
    [pscustomobject]@{
        title = "P4 W23-24: Implement locking mechanisms"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 23-24 - Concurrency Control`n`n### Objective`nImplement locking mechanisms.`n`n### Acceptance Criteria`n- Lock manager supports shared/exclusive semantics`n- Conflicting operations are serialized correctly"
    }
    [pscustomobject]@{
        title = "P4 W23-24: Add MVCC support"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 23-24 - Concurrency Control`n`n### Objective`nAdd Multi-Version Concurrency Control (MVCC).`n`n### Acceptance Criteria`n- Readers can access consistent snapshots without blocking writers`n- Version visibility rules are documented and tested"
    }
    [pscustomobject]@{
        title = "P4 W23-24: Support concurrent transactions"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 23-24 - Concurrency Control`n`n### Objective`nSupport concurrent transactions.`n`n### Acceptance Criteria`n- Concurrent workloads execute without violating consistency`n- Isolation behavior matches selected isolation level(s)"
    }
    [pscustomobject]@{
        title = "P4 W23-24: Handle deadlock detection and resolution"
        body  = "## Phase 4: Indexing and Performance`n## Section: Week 23-24 - Concurrency Control`n`n### Objective`nHandle deadlock detection and resolution.`n`n### Acceptance Criteria`n- Deadlocks are detected within bounded time`n- Victim selection and rollback policy are implemented and tested"
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

$labels = @("phase-4", "indexing-performance")

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

    Write-Host "Created $($issues.Count) Phase 4 issues under milestone '$MilestoneTitle'." -ForegroundColor Green
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
