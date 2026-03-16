param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "User Interface Development",
    [string]$OutFile = "phase6-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

$issues = @(
    [pscustomobject]@{
        title = "P6 W29-30: Set up React development environment"
        body  = "## Phase 6: User Interface Development`n## Section: Week 29-30 - React UI Foundation`n`n### Objective`nSet up React development environment.`n`n### Acceptance Criteria`n- React project scaffolded with TypeScript and build tooling`n- Local development server and production build both work"
    }
    [pscustomobject]@{
        title = "P6 W29-30: Create basic UI layout and navigation"
        body  = "## Phase 6: User Interface Development`n## Section: Week 29-30 - React UI Foundation`n`n### Objective`nCreate basic UI layout and navigation.`n`n### Acceptance Criteria`n- Main layout includes workspace regions (sidebar, editor, result area)`n- Navigation supports switching between key views"
    }
    [pscustomobject]@{
        title = "P6 W29-30: Implement connection management"
        body  = "## Phase 6: User Interface Development`n## Section: Week 29-30 - React UI Foundation`n`n### Objective`nImplement connection management.`n`n### Acceptance Criteria`n- UI supports API endpoint configuration and session connection/disconnection`n- Connection state and errors are clearly shown to the user"
    }
    [pscustomobject]@{
        title = "P6 W29-30: Build SQL editor component"
        body  = "## Phase 6: User Interface Development`n## Section: Week 29-30 - React UI Foundation`n`n### Objective`nBuild SQL editor component.`n`n### Acceptance Criteria`n- SQL text input supports executing queries against the API`n- Editor integrates with query result display"
    }
    [pscustomobject]@{
        title = "P6 W31-32: Implement table explorer"
        body  = "## Phase 6: User Interface Development`n## Section: Week 31-32 - Core UI Components`n`n### Objective`nImplement table explorer.`n`n### Acceptance Criteria`n- Tables are listed and selectable from the UI`n- Table metadata is accessible from explorer interactions"
    }
    [pscustomobject]@{
        title = "P6 W31-32: Create query result viewer"
        body  = "## Phase 6: User Interface Development`n## Section: Week 31-32 - Core UI Components`n`n### Objective`nCreate query result viewer.`n`n### Acceptance Criteria`n- Query results render in a readable grid/table format`n- Large result sets are handled with stable performance"
    }
    [pscustomobject]@{
        title = "P6 W31-32: Add schema visualization"
        body  = "## Phase 6: User Interface Development`n## Section: Week 31-32 - Core UI Components`n`n### Objective`nAdd schema visualization.`n`n### Acceptance Criteria`n- Schema relationships can be viewed in a dedicated UI area`n- Schema view updates when selected database objects change"
    }
    [pscustomobject]@{
        title = "P6 W31-32: Build data editing capabilities"
        body  = "## Phase 6: User Interface Development`n## Section: Week 31-32 - Core UI Components`n`n### Objective`nBuild data editing capabilities.`n`n### Acceptance Criteria`n- UI supports insert/update/delete operations through validated forms`n- Data edit actions show success/failure feedback"
    }
    [pscustomobject]@{
        title = "P6 W33-34: Add query history and favorites"
        body  = "## Phase 6: User Interface Development`n## Section: Week 33-34 - Advanced UI Features`n`n### Objective`nAdd query history and favorites.`n`n### Acceptance Criteria`n- Users can revisit previous queries`n- Frequently used queries can be saved as favorites"
    }
    [pscustomobject]@{
        title = "P6 W33-34: Implement export/import functionality"
        body  = "## Phase 6: User Interface Development`n## Section: Week 33-34 - Advanced UI Features`n`n### Objective`nImplement export/import functionality.`n`n### Acceptance Criteria`n- Query/table data can be exported in supported formats`n- UI supports importing supported data files with validation"
    }
    [pscustomobject]@{
        title = "P6 W33-34: Create user preferences"
        body  = "## Phase 6: User Interface Development`n## Section: Week 33-34 - Advanced UI Features`n`n### Objective`nCreate user preferences.`n`n### Acceptance Criteria`n- Users can persist key UI preferences`n- Preferences load correctly when the app restarts"
    }
    [pscustomobject]@{
        title = "P6 W33-34: Add performance monitoring dashboard"
        body  = "## Phase 6: User Interface Development`n## Section: Week 33-34 - Advanced UI Features`n`n### Objective`nAdd performance monitoring dashboard.`n`n### Acceptance Criteria`n- UI displays key performance indicators from API/database`n- Metrics update in near real-time or on refresh"
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

$labels = @("phase-6", "ui")

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

    Write-Host "Created $($issues.Count) Phase 6 issues under milestone '$MilestoneTitle'." -ForegroundColor Green
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
