param(
    [switch]$StartApp,
    [switch]$ClearLog,
    [string]$Filter = "",
    [int]$TailLines = 200,
    [string]$AppPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($AppPath)) {
    $AppPath = Join-Path $PSScriptRoot "build\\bin\\Debug\\SimpleSpaces.exe"
}

$logRoot = Join-Path $env:LOCALAPPDATA "SimpleSpaces"
$logPath = Join-Path $logRoot "debug.log"

if (-not (Test-Path $logRoot)) {
    New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
}

if ($ClearLog -and (Test-Path $logPath)) {
    Remove-Item $logPath -Force
}

if (-not (Test-Path $logPath)) {
    New-Item -ItemType File -Path $logPath -Force | Out-Null
}

if ($StartApp) {
    $running = Get-Process SimpleSpaces -ErrorAction SilentlyContinue
    if (-not $running) {
        if (-not (Test-Path $AppPath)) {
            throw "SimpleSpaces executable not found at: $AppPath"
        }
        Start-Process -FilePath $AppPath | Out-Null
    }
}

Write-Host "Monitoring log: $logPath"
if (-not [string]::IsNullOrWhiteSpace($Filter)) {
    Write-Host "Filter regex: $Filter"
    Get-Content -Path $logPath -Tail $TailLines -Wait |
        Where-Object { $_ -match $Filter }
}
else {
    Write-Host "No filter enabled (showing full runtime log stream)."
    Get-Content -Path $logPath -Tail $TailLines -Wait
}
