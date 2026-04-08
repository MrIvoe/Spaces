param(
    [string]$ExePath = "C:\Users\MrIvo\Github\Spaces\build\bin\Debug\Spaces.exe",
    [string]$SettingsPath = "$env:LOCALAPPDATA\SimpleSpaces\Spaces\settings.json",
    [int]$LaunchWaitSeconds = 2,
    [int]$ShutdownWaitSeconds = 1
)

$ErrorActionPreference = "Stop"
$testResults = @()

function Ensure-ParentObject {
    param(
        [object]$Root,
        [string[]]$Parts
    )

    $node = $Root
    foreach ($part in $Parts) {
        if ($null -eq $node.$part) {
            $node | Add-Member -MemberType NoteProperty -Name $part -Value ([pscustomobject]@{}) -Force
        }
        $node = $node.$part
    }

    return $node
}

function Get-JsonPathValue {
    param(
        [object]$Root,
        [string]$Path
    )

    $parts = $Path -split "\."
    $node = $Root
    foreach ($part in $parts) {
        if ($null -eq $node) {
            return $null
        }
        $node = $node.$part
    }

    return $node
}

function Set-JsonPathValue {
    param(
        [object]$Root,
        [string]$Path,
        [string]$Value
    )

    $parts = $Path -split "\."
    if ($parts.Count -eq 1) {
        $Root | Add-Member -MemberType NoteProperty -Name $parts[0] -Value $Value -Force
        return
    }

    $parent = Ensure-ParentObject -Root $Root -Parts $parts[0..($parts.Count - 2)]
    $parent | Add-Member -MemberType NoteProperty -Name $parts[-1] -Value $Value -Force
}

function New-TestResult {
    param(
        [string]$Test,
        [string]$Status,
        [string]$Message
    )

    return [pscustomobject]@{
        Test = $Test
        Status = $Status
        Message = $Message
    }
}

function Get-FlatValueKey {
    param(
        [string]$Path
    )

    if ($Path.StartsWith("values.")) {
        return $Path.Substring(7)
    }

    return $null
}

function Get-ValueNode {
    param(
        [object]$Root
    )

    if ($null -eq $Root.values) {
        $Root | Add-Member -MemberType NoteProperty -Name values -Value ([pscustomobject]@{}) -Force
    }

    return $Root.values
}

function Get-JsonValue {
    param(
        [object]$Root,
        [string]$Path
    )

    $flatKey = Get-FlatValueKey -Path $Path
    if ($null -ne $flatKey) {
        $valuesNode = Get-ValueNode -Root $Root
        $prop = $valuesNode.PSObject.Properties[$flatKey]
        if ($null -eq $prop) {
            return $null
        }
        return $prop.Value
    }

    return Get-JsonPathValue -Root $Root -Path $Path
}

function Set-JsonValue {
    param(
        [object]$Root,
        [string]$Path,
        [string]$Value
    )

    $flatKey = Get-FlatValueKey -Path $Path
    if ($null -ne $flatKey) {
        $valuesNode = Get-ValueNode -Root $Root
        $valuesNode | Add-Member -MemberType NoteProperty -Name $flatKey -Value $Value -Force
        return
    }

    Set-JsonPathValue -Root $Root -Path $Path -Value $Value
}

function Wait-ForJsonValue {
    param(
        [string]$Path,
        [int]$Attempts = 50,
        [int]$DelayMilliseconds = 300
    )

    for ($i = 0; $i -lt $Attempts; $i++) {
        $snapshot = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        $value = Get-JsonValue -Root $snapshot -Path $Path
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }

        Start-Sleep -Milliseconds $DelayMilliseconds
    }

    return ""
}

function Wait-ForJsonValueEquals {
    param(
        [string]$Path,
        [string]$Expected,
        [int]$Attempts = 50,
        [int]$DelayMilliseconds = 300
    )

    for ($i = 0; $i -lt $Attempts; $i++) {
        $snapshot = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        $value = Get-JsonValue -Root $snapshot -Path $Path
        if ($value -eq $Expected) {
            return $true
        }

        Start-Sleep -Milliseconds $DelayMilliseconds
    }

    return $false
}

function Test-SettingsPersistence {
    param(
        [string]$TestName,
        [string]$SettingPath,
        [string]$ExpectedValue
    )

    Write-Host "`n=== Test: $TestName ===" -ForegroundColor Cyan

    try {
        if (-not (Test-Path $SettingsPath)) {
            throw "Settings file not found: $SettingsPath"
        }
        Write-Host "[OK] Settings file exists"

        $settings = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        Write-Host "[OK] Settings file parsed"

        $currentValue = Get-JsonValue -Root $settings -Path $SettingPath
        Write-Host "[OK] Current value: $currentValue"

        Set-JsonValue -Root $settings -Path $SettingPath -Value $ExpectedValue
        $settings | ConvertTo-Json -Depth 20 | Set-Content $SettingsPath -Encoding UTF8
        Write-Host "[OK] Updated setting to: $ExpectedValue"

        $proc = Start-Process -FilePath $ExePath -PassThru -ErrorAction SilentlyContinue
        if ($null -eq $proc) {
            throw "Failed to start Spaces.exe"
        }

        Start-Sleep -Seconds $LaunchWaitSeconds
        Write-Host "[OK] Application started (PID: $($proc.Id))"

        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue | Out-Null
            Start-Sleep -Seconds $ShutdownWaitSeconds
        }
        Write-Host "[OK] Application stopped"

        $persisted = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        $persistedValue = Get-JsonValue -Root $persisted -Path $SettingPath

        if ($persistedValue -ne $ExpectedValue) {
            throw "Setting was not persisted: expected '$ExpectedValue', got '$persistedValue'"
        }

        Write-Host "[OK] Setting persisted correctly: $persistedValue" -ForegroundColor Green
        return (New-TestResult -Test $TestName -Status "PASS" -Message "Setting persisted as expected")
    }
    catch {
        Write-Host "[XX] Test failed: $_" -ForegroundColor Red
        return (New-TestResult -Test $TestName -Status "FAIL" -Message $_.Exception.Message)
    }
}

function Test-ManagedThemeNormalization {
    param(
        [string]$TestName
    )

    Write-Host "`n=== Test: $TestName ===" -ForegroundColor Cyan

    try {
        if (-not (Test-Path $SettingsPath)) {
            throw "Settings file not found: $SettingsPath"
        }

        $settings = Get-Content $SettingsPath -Raw | ConvertFrom-Json

        # Force migration path and inject non-canonical values.
        Set-JsonValue -Root $settings -Path "values.theme.migration_v2_complete" -Value "false"
        Set-JsonValue -Root $settings -Path "values.theme.source" -Value "manual_override"
        Set-JsonValue -Root $settings -Path "values.theme.win32.theme_id" -Value "Arctic_Glass"
        Set-JsonValue -Root $settings -Path "values.theme.preset" -Value "Arctic_Glass"
        Set-JsonValue -Root $settings -Path "values.theme.win32.display_name" -Value ""
        Set-JsonValue -Root $settings -Path "values.theme.win32.catalog_version" -Value ""
        $settings | ConvertTo-Json -Depth 20 | Set-Content $SettingsPath -Encoding UTF8
        Write-Host "[OK] Seeded non-canonical managed theme keys"

        $proc = Start-Process -FilePath $ExePath -PassThru -ErrorAction SilentlyContinue
        if ($null -eq $proc) {
            throw "Failed to start Spaces.exe"
        }

        Start-Sleep -Seconds $LaunchWaitSeconds

        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue | Out-Null
            Start-Sleep -Seconds $ShutdownWaitSeconds
        }

        $persisted = Get-Content $SettingsPath -Raw | ConvertFrom-Json

        $source = Get-JsonValue -Root $persisted -Path "values.theme.source"
        $themeId = Get-JsonValue -Root $persisted -Path "values.theme.win32.theme_id"
        $preset = Get-JsonValue -Root $persisted -Path "values.theme.preset"
        $displayName = Get-JsonValue -Root $persisted -Path "values.theme.win32.display_name"
        $catalogVersion = Get-JsonValue -Root $persisted -Path "values.theme.win32.catalog_version"

        if ($source -ne "win32_theme_system") {
            throw "Expected values.theme.source=win32_theme_system, got '$source'"
        }
        if ($themeId -ne "arctic-glass") {
            throw "Expected values.theme.win32.theme_id=arctic-glass, got '$themeId'"
        }
        if ($preset -ne "arctic-glass") {
            throw "Expected values.theme.preset=arctic-glass, got '$preset'"
        }
        if ([string]::IsNullOrWhiteSpace($displayName)) {
            throw "Expected values.theme.win32.display_name to be populated"
        }
        if ([string]::IsNullOrWhiteSpace($catalogVersion)) {
            throw "Expected values.theme.win32.catalog_version to be populated"
        }

        Write-Host "[OK] Managed theme keys normalized as expected" -ForegroundColor Green
        return (New-TestResult -Test $TestName -Status "PASS" -Message "Managed theme normalization passed")
    }
    catch {
        Write-Host "[XX] Test failed: $_" -ForegroundColor Red
        return (New-TestResult -Test $TestName -Status "FAIL" -Message $_.Exception.Message)
    }
}

function Test-InvalidThemeIdFallback {
    param(
        [string]$TestName
    )

    Write-Host "`n=== Test: $TestName ===" -ForegroundColor Cyan

    try {
        if (-not (Test-Path $SettingsPath)) {
            throw "Settings file not found: $SettingsPath"
        }

        $settings = Get-Content $SettingsPath -Raw | ConvertFrom-Json

        # Force migration path and seed unknown theme id.
        Set-JsonValue -Root $settings -Path "values.theme.migration_v2_complete" -Value "false"
        Set-JsonValue -Root $settings -Path "values.theme.source" -Value "win32_theme_system"
        Set-JsonValue -Root $settings -Path "values.theme.win32.theme_id" -Value "mystery_theme"
        Set-JsonValue -Root $settings -Path "values.theme.preset" -Value "mystery_theme"
        Set-JsonValue -Root $settings -Path "values.theme.win32.display_name" -Value ""
        Set-JsonValue -Root $settings -Path "values.theme.win32.catalog_version" -Value ""
        $settings | ConvertTo-Json -Depth 20 | Set-Content $SettingsPath -Encoding UTF8
        Write-Host "[OK] Seeded unknown theme id for fallback validation"

        $proc = Start-Process -FilePath $ExePath -PassThru -ErrorAction SilentlyContinue
        if ($null -eq $proc) {
            throw "Failed to start Spaces.exe"
        }

        Start-Sleep -Seconds $LaunchWaitSeconds
        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue | Out-Null
            Start-Sleep -Seconds $ShutdownWaitSeconds
        }

        $persisted = Get-Content $SettingsPath -Raw | ConvertFrom-Json

        $themeId = Get-JsonValue -Root $persisted -Path "values.theme.win32.theme_id"
        $preset = Get-JsonValue -Root $persisted -Path "values.theme.preset"
        $displayName = Get-JsonValue -Root $persisted -Path "values.theme.win32.display_name"
        $catalogVersion = Get-JsonValue -Root $persisted -Path "values.theme.win32.catalog_version"

        if ($themeId -ne "graphite-office") {
            throw "Expected values.theme.win32.theme_id=graphite-office, got '$themeId'"
        }
        if ($preset -ne "graphite-office") {
            throw "Expected values.theme.preset=graphite-office, got '$preset'"
        }
        if ($displayName -ne "Graphite Office") {
            throw "Expected values.theme.win32.display_name=Graphite Office, got '$displayName'"
        }
        if ($catalogVersion -ne "2026.04.06") {
            throw "Expected values.theme.win32.catalog_version=2026.04.06, got '$catalogVersion'"
        }

        Write-Host "[OK] Unknown theme id fallback to graphite-office verified" -ForegroundColor Green
        return (New-TestResult -Test $TestName -Status "PASS" -Message "Unknown theme id fallback passed")
    }
    catch {
        Write-Host "[XX] Test failed: $_" -ForegroundColor Red
        return (New-TestResult -Test $TestName -Status "FAIL" -Message $_.Exception.Message)
    }
}

Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Settings Persistence Test Suite" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Settings Path: $SettingsPath"
Write-Host "Executable: $ExePath"

if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: Executable not found: $ExePath" -ForegroundColor Red
    exit 1
}

$settingsDir = Split-Path -Path $SettingsPath -Parent
if (-not (Test-Path $settingsDir)) {
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
}
if (-not (Test-Path $SettingsPath)) {
    '{"version":1,"values":{}}' | Set-Content $SettingsPath -Encoding UTF8
}

Get-Process -Name "Spaces" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

$testResults += ,(Test-SettingsPersistence -TestName "Text Scale Persistence" -SettingPath "appearance.theme.text_scale_percent" -ExpectedValue "120")
$testResults += ,(Test-SettingsPersistence -TestName "Custom Marker Persistence" -SettingPath "values.test.persistence_marker" -ExpectedValue "ok")
$testResults += ,(Test-ManagedThemeNormalization -TestName "Managed Theme Normalization")
$testResults += ,(Test-InvalidThemeIdFallback -TestName "Invalid Theme Id Fallback")

Write-Host "`n========================================" -ForegroundColor Yellow
Write-Host "Test Summary" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow

$passed = ($testResults | Where-Object { $_.Status -eq "PASS" }).Count
$failed = ($testResults | Where-Object { $_.Status -eq "FAIL" }).Count
$total = $testResults.Count

$testResults | Format-Table -AutoSize Test, Status, Message

Write-Host "`nTotal:  $total tests" -ForegroundColor Cyan
Write-Host "Passed: $passed tests" -ForegroundColor Green
Write-Host "Failed: $failed tests" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })

if ($failed -eq 0) {
    Write-Host "`nAll persistence tests passed!" -ForegroundColor Green
    exit 0
}

Write-Host "`nSome tests failed" -ForegroundColor Red
exit 1
