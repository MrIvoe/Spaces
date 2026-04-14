param(
    [switch]$SkipDebugBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    Write-Host "`n==> $Name" -ForegroundColor Cyan
    & $Action
    Write-Host "OK: $Name" -ForegroundColor Green
}

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }
}

function Find-Iscc {
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "Inno Setup compiler (ISCC.exe) not found. Install Inno Setup 6 and retry."
}

function Get-InstallerVersion {
    param([Parameter(Mandatory = $true)][string]$InstallerScriptPath)

    $match = Select-String -Path $InstallerScriptPath -Pattern '^#define\s+MyAppVersion\s+"([0-9.]+)"'
    if (-not $match) {
        throw "Could not find MyAppVersion in $InstallerScriptPath"
    }

    return $match.Matches[0].Groups[1].Value
}

function Assert-NoForbiddenPathsInGitStatus {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $statusLines = git -C $RepoRoot status --porcelain
    if (-not $statusLines) {
        return
    }

    $forbiddenPrefixes = @(
        "build/",
        "out/",
        ".vs/",
        "installer/output/",
        "docs/wiki/",
        "crashdumps/"
    )

    $violations = @()
    foreach ($line in $statusLines) {
        if ($line.Length -lt 4) {
            continue
        }

        $path = $line.Substring(3).Replace('\\', '/')
        foreach ($prefix in $forbiddenPrefixes) {
            if ($path.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                $violations += $path
                break
            }
        }
    }

    if ($violations.Count -gt 0) {
        $list = ($violations | Sort-Object -Unique) -join "`n - "
        throw "Forbidden non-runtime paths detected in git status:`n - $list`nClean these before publishing."
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$solutionPath = Join-Path $repoRoot "build\Spaces.slnx"
$installerScript = Join-Path $repoRoot "installer\Spaces.iss"
$releaseExe = Join-Path $repoRoot "build\bin\Release\Spaces.exe"
$iscc = Find-Iscc
$installerVersion = Get-InstallerVersion -InstallerScriptPath $installerScript
$expectedInstaller = Join-Path $repoRoot "installer\output\Spaces.$installerVersion.exe"

Invoke-Step -Name "Validate required files" -Action {
    if (-not (Test-Path $solutionPath)) { throw "Missing solution file: $solutionPath" }
    if (-not (Test-Path $installerScript)) { throw "Missing installer script: $installerScript" }
}

Invoke-Step -Name "Build app (Debug)" -Action {
    if ($SkipDebugBuild) {
        Write-Host "Skipped Debug build by request." -ForegroundColor Yellow
        return
    }

    Invoke-Native "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" $solutionPath "/p:Configuration=Debug" "/v:minimal"
}

Invoke-Step -Name "Build app (Release)" -Action {
    Invoke-Native "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" $solutionPath "/p:Configuration=Release" "/v:minimal"
    if (-not (Test-Path $releaseExe)) {
        throw "Release executable not found: $releaseExe"
    }
}

Invoke-Step -Name "Run HostCoreTests" -Action {
    if ($SkipTests) {
        Write-Host "Skipped tests by request." -ForegroundColor Yellow
        return
    }

    Invoke-Native (Join-Path $repoRoot "build\Debug\HostCoreTests.exe")
}

Invoke-Step -Name "Build installer" -Action {
    $env:BUILD_OUTPUT_DIR = Join-Path $repoRoot "build\bin\Release"
    Invoke-Native $iscc $installerScript
    if (-not (Test-Path $expectedInstaller)) {
        throw "Expected installer artifact not found: $expectedInstaller"
    }
}

Invoke-Step -Name "Check git status for non-runtime uploads" -Action {
    Assert-NoForbiddenPathsInGitStatus -RepoRoot $repoRoot
}

Write-Host "`nRelease checklist passed." -ForegroundColor Green
Write-Host "Installer artifact: $expectedInstaller" -ForegroundColor Green
