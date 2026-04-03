[CmdletBinding()]
param(
    [string]$ChecklistPath = (Join-Path $PSScriptRoot "MANUAL_RUNTIME_SMOKE_TEST.md"),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "manual-smoke-results")
)

$ErrorActionPreference = "Stop"

function Get-SectionMap {
    param(
        [string[]]$Lines
    )

    $sections = @()
    for ($i = 0; $i -lt $Lines.Length; $i++) {
        if ($Lines[$i] -match '^##\s+(.+)$') {
            $sections += [PSCustomObject]@{
                Title = $matches[1].Trim()
                Line = $i + 1
            }
        }
    }

    return $sections
}

if (-not (Test-Path -LiteralPath $ChecklistPath)) {
    throw "Checklist file not found: $ChecklistPath"
}

$content = Get-Content -LiteralPath $ChecklistPath
$sections = Get-SectionMap -Lines $content
if ($sections.Count -eq 0) {
    throw "No sections found in checklist: $ChecklistPath"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultPath = Join-Path $OutputDirectory "manual-smoke-$timestamp.md"

$header = @(
    "# Manual Smoke Run $timestamp",
    "",
    "- Checklist: $ChecklistPath",
    "- Host: $(Get-Location)",
    "- Runner: Run-ManualSmoke.ps1",
    "",
    "| Section | Status | Notes |",
    "| --- | --- | --- |"
)
Set-Content -LiteralPath $resultPath -Value $header

Write-Host "Manual smoke runner started."
Write-Host "Checklist: $ChecklistPath"
Write-Host "Result file: $resultPath"
Write-Host ""
Write-Host "Status options: pass, fail, skip"
Write-Host ""

$codeCmd = Get-Command code -ErrorAction SilentlyContinue

foreach ($section in $sections) {
    Write-Host "============================================================"
    Write-Host "Section: $($section.Title)"

    if ($codeCmd) {
        & $codeCmd.Source -g "$ChecklistPath`:$($section.Line)"
        Write-Host "Opened in VS Code at line $($section.Line)."
    } else {
        Write-Host "VS Code CLI not found. Section line: $($section.Line)"
    }

    $status = ""
    while ($status -notin @("pass", "fail", "skip")) {
        $status = (Read-Host "Enter status (pass/fail/skip)").Trim().ToLowerInvariant()
    }

    $notes = Read-Host "Notes (optional)"
    if ([string]::IsNullOrWhiteSpace($notes)) {
        $notes = "-"
    } else {
        $notes = $notes.Replace("|", "\\|")
    }

    Add-Content -LiteralPath $resultPath -Value "| $($section.Title) | $status | $notes |"
}

Add-Content -LiteralPath $resultPath -Value ""
Add-Content -LiteralPath $resultPath -Value "Completed: $(Get-Date -Format o)"

Write-Host ""
Write-Host "Manual smoke run complete."
Write-Host "Report saved to: $resultPath"
