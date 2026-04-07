#include "Win32Helpers.h"
#include "core/SettingsStore.h"
#include "core/ThemeApplyPipeline.h"
#include "core/ThemeMigrationService.h"

#include <iostream>
#include <filesystem>
#include <string>

static int Fail(const char* message)
{
    std::cerr << "FAIL [TelemetrySnapshot]: " << message << "\n";
    return 1;
}

static std::filesystem::path MakeTempPath(const char* tag)
{
    namespace fs = std::filesystem;
    return fs::temp_directory_path() / ("tsd_" + std::string(tag) + "_" + std::to_string(GetTickCount64()) + ".json");
}

// RunTelemetrySnapshotDiagnostics
//
// Drives exactly one example of each instrumented lifecycle event, then
// prints a structured counter table to stdout.  Intended for release smoke
// runs: the output provides immediate visual confirmation that every
// telemetry path is reachable, and the function returns non-zero if any
// counter stays at zero (which would indicate a broken instrumentation path).
int RunTelemetrySnapshotDiagnostics()
{
    Win32Helpers::ResetTelemetryCounters();

    // --- Migration event ---
    // A file-backed store is required so Save() succeeds and the counter
    // increments.  An underscore-form theme_id triggers normalisation.
    {
        SettingsStore store;
        const auto tempPath = MakeTempPath("migration");
        store.Load(tempPath);
        store.Set(L"theme.source", L"win32_theme_system");
        store.Set(L"theme.win32.theme_id", L"graphite_office"); // underscore → normalised to kebab
        ThemeMigrationService migration(&store);
        migration.Migrate();
    }

    // --- Apply-success event ---
    // A file-backed store is required so Save() succeeds during apply.
    {
        SettingsStore store;
        const auto tempPath = MakeTempPath("apply_success");
        store.Load(tempPath);
        store.Set(L"theme.source", L"win32_theme_system");
        store.Set(L"theme.win32.theme_id", L"harbor-blue");
        ThemeApplyPipeline pipeline(&store);
        pipeline.ApplyTheme(L"harbor-blue");
    }

    // --- Apply-fallback event ---
    // An unknown ID cannot be resolved; the pipeline falls back to
    // graphite-office and increments theme.apply.fallback.
    {
        SettingsStore store;
        store.Set(L"theme.source", L"win32_theme_system");
        store.Set(L"theme.win32.theme_id", L"graphite-office");
        ThemeApplyPipeline pipeline(&store);
        pipeline.ApplyTheme(L"unknown-smoke-theme-xyz");
    }

    // --- Apply-failure event ---
    // An empty theme ID is rejected before lookup and increments
    // theme.apply.failure.
    {
        SettingsStore store;
        ThemeApplyPipeline pipeline(&store);
        pipeline.ApplyTheme(L"");
    }

    const uint64_t migration = Win32Helpers::GetTelemetryCounterValue(L"theme.migration");
    const uint64_t success   = Win32Helpers::GetTelemetryCounterValue(L"theme.apply.success");
    const uint64_t fallback  = Win32Helpers::GetTelemetryCounterValue(L"theme.apply.fallback");
    const uint64_t failure   = Win32Helpers::GetTelemetryCounterValue(L"theme.apply.failure");

    std::cout << "\n=== Telemetry Smoke Snapshot ===\n";
    std::cout << "  theme.migration      : " << migration << "\n";
    std::cout << "  theme.apply.success  : " << success   << "\n";
    std::cout << "  theme.apply.fallback : " << fallback  << "\n";
    std::cout << "  theme.apply.failure  : " << failure   << "\n";
    std::cout << "================================\n\n";

    if (migration < 1)
        return Fail("theme.migration counter should be >= 1 after migration cycle");
    if (success < 1)
        return Fail("theme.apply.success counter should be >= 1 after valid apply");
    if (fallback < 1)
        return Fail("theme.apply.fallback counter should be >= 1 after unknown-id apply");
    if (failure < 1)
        return Fail("theme.apply.failure counter should be >= 1 after empty-id apply");

    return 0;
}
