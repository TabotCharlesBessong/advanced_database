param(
    [string]$Owner = "TabotCharlesBessong",
    [string]$RepoName = "advanced_database",
    [string]$MilestoneTitle = "API Layer Development",
    [string]$OutFile = "auth-fix-issue-links.txt",
    [switch]$OpenInBrowser
)

$ErrorActionPreference = "Stop"

$repoUrl = "https://github.com/$Owner/$RepoName"
$repoSlug = "$Owner/$RepoName"

$issues = @(
    [pscustomobject]@{
        title = "Auth Fix: Add signup endpoint and onboarding-compatible account flow"
        body  = "## Context`nFollow-up authentication fix after Phase 5 API/UI integration.`n`n### Scope`n- Add signup endpoint for new account creation`n- Support onboarding flow in UI and API`n`n### Acceptance Criteria`n- `POST /auth/signup` exists and creates users`n- Signup returns JWT token and user payload`n- Duplicate usernames return clear conflict response"
    }
    [pscustomobject]@{
        title = "Auth Fix: Add password visibility toggle in UI"
        body  = "## Context`nImprove authentication UX in UI connection panel.`n`n### Scope`n- Add show/hide toggle for password field`n- Add show/hide toggle for confirm-password in signup mode`n`n### Acceptance Criteria`n- Users can toggle password visibility without losing input`n- Toggle works in both login and signup forms"
    }
    [pscustomobject]@{
        title = "Auth Fix: Add onboarding flow with login/signup mode switch"
        body  = "## Context`nUsers reported login friction and requested guided onboarding.`n`n### Scope`n- Add onboarding guidance in connection panel`n- Add login/signup mode switch`n- Add confirm-password validation in signup mode`n`n### Acceptance Criteria`n- User can complete account creation and connect in one guided flow`n- Validation errors are user-friendly and actionable"
    }
    [pscustomobject]@{
        title = "Auth Fix: Enable CORS for browser-based UI authentication"
        body  = "## Context`nBrowser auth calls from React UI require CORS support in API.`n`n### Scope`n- Add CORS headers for allowed methods and headers`n- Handle OPTIONS preflight requests`n`n### Acceptance Criteria`n- Browser requests from UI to API login/signup endpoints succeed`n- No CORS-related blocks on standard auth requests"
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

$labels = @("auth", "bugfix")

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

    Write-Host "Created $($issues.Count) authentication-fix issues under milestone '$MilestoneTitle'." -ForegroundColor Green
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
