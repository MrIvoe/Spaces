param(
    [switch]$SkipDebugBuild,
    [switch]$SkipTests,
    [string]$Version = "",     # Explicit version override, e.g. "1.01.012"
    [switch]$NoVersionBump     # Skip auto-increment; rebuild and repackage at current version
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

function Step-PatchVersion {
    param([Parameter(Mandatory = $true)][string]$CurrentVersion)

    $parts = $CurrentVersion.Split('.')
    if ($parts.Count -ne 3) {
        throw "Unexpected version format '$CurrentVersion'. Expected X.YY.ZZZ (e.g. 1.01.010)."
    }

    $patchWidth = $parts[2].Length
    $newPatch = ([int]$parts[2] + 1).ToString().PadLeft($patchWidth, '0')
    return "$($parts[0]).$($parts[1]).$newPatch"
}

function Set-VersionInFiles {
    param(
        [Parameter(Mandatory = $true)][string]$AppVersionPath,
        [Parameter(Mandatory = $true)][string]$InstallerScriptPath,
        [Parameter(Mandatory = $true)][string]$NewVersion
    )

    $avContent = Get-Content -LiteralPath $AppVersionPath -Raw
    $avUpdated = $avContent -replace '(kVersion\[\]\s*=\s*L")([0-9.]+)(")', "`${1}$NewVersion`${3}"
    [System.IO.File]::WriteAllText($AppVersionPath, $avUpdated, [System.Text.UTF8Encoding]::new($false))

    $issContent = Get-Content -LiteralPath $InstallerScriptPath -Raw
    $issUpdated = $issContent -replace '(#define\s+MyAppVersion\s+")([0-9.]+)(")', "`${1}$NewVersion`${3}"
    [System.IO.File]::WriteAllText($InstallerScriptPath, $issUpdated, [System.Text.UTF8Encoding]::new($false))
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
$appVersionPath = Join-Path $repoRoot "src\AppVersion.h"
$releaseExe = Join-Path $repoRoot "build\bin\Release\Spaces.exe"
$iscc = Find-Iscc

# Read current version from AppVersion.h (single source of truth)
$currentVersionMatch = Select-String -LiteralPath $appVersionPath -Pattern 'kVersion\[\]\s*=\s*L"([0-9.]+)"'
if (-not $currentVersionMatch) {
    throw "Could not find kVersion in $appVersionPath"
}
$currentVersion = $currentVersionMatch.Matches[0].Groups[1].Value

# Determine target version
if ($Version -ne "") {
    $targetVersion = $Version
    Write-Host "Version override: $currentVersion → $targetVersion" -ForegroundColor Magenta
} elseif ($NoVersionBump) {
    $targetVersion = $currentVersion
    Write-Host "Version: $currentVersion (no bump; repackaging current)" -ForegroundColor Yellow
} else {
    $targetVersion = Step-PatchVersion -CurrentVersion $currentVersion
    Write-Host "Version: $currentVersion → $targetVersion (auto-increment)" -ForegroundColor Magenta
}

$installerVersion = $targetVersion
$expectedInstaller = Join-Path $repoRoot "installer\output\Spaces.$installerVersion.exe"

Invoke-Step -Name "Bump version to $targetVersion" -Action {
    if ($targetVersion -eq $currentVersion) {
        Write-Host "No version change ($currentVersion). Skipping file updates." -ForegroundColor Yellow
        return
    }
    Set-VersionInFiles -AppVersionPath $appVersionPath -InstallerScriptPath $installerScript -NewVersion $targetVersion
    Write-Host "  src/AppVersion.h     → $targetVersion"
    Write-Host "  installer/Spaces.iss → $targetVersion"
}

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
Write-Host "Version:            $installerVersion" -ForegroundColor Green
Write-Host "Installer artifact: $expectedInstaller" -ForegroundColor Green

Write-Host "`n==> Next Steps" -ForegroundColor Cyan
if ($targetVersion -ne $currentVersion) {
    Write-Host "1. Commit the version bump:" -ForegroundColor White
    Write-Host "     git add src/AppVersion.h installer/Spaces.iss" -ForegroundColor DarkGray
    Write-Host "     git commit -m `"release: Bump version to $installerVersion`"" -ForegroundColor DarkGray
} else {
    Write-Host "1. Version was not bumped. Re-run without -NoVersionBump to increment." -ForegroundColor Yellow
}
Write-Host "2. Update CHANGELOG.md with changes for $installerVersion, then commit:" -ForegroundColor White
Write-Host "     git add CHANGELOG.md" -ForegroundColor DarkGray
Write-Host "     git commit -m `"docs: CHANGELOG for $installerVersion`"" -ForegroundColor DarkGray
Write-Host "3. Push and tag the release:" -ForegroundColor White
Write-Host "     git push origin main" -ForegroundColor DarkGray
Write-Host "     git tag v$installerVersion" -ForegroundColor DarkGray
Write-Host "     git push origin v$installerVersion" -ForegroundColor DarkGray
Write-Host "4. Upload installer to GitHub Releases:" -ForegroundColor White
Write-Host "     $expectedInstaller" -ForegroundColor Yellow
Write-Host "5. Smoke test the installer on a clean profile before announcing." -ForegroundColor White
