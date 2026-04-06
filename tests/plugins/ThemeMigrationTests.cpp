#include <string>
#include <iostream>
#include <filesystem>
#include <cstdio>
#include <ctime>

#include "core/ThemeMigrationService.h"
#include "core/SettingsStore.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }

    // Generate a unique temp file path
    std::filesystem::path GetUniqueTempPath()
    {
        // Create a unique temp file name based on timestamp and random number
        static int testCounter = 0;
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::filesystem::path result = tempDir / ("test_theme_store_" + std::to_string(testCounter++) + "_" + std::to_string(std::time(nullptr)) + ".json");
        return result;
    }

    // RAII helper to clean up temp files
    class TempFileGuard
    {
    public:
        TempFileGuard(const std::filesystem::path& path) : m_path(path) {}
        ~TempFileGuard()
        {
            try
            {
                if (std::filesystem::exists(m_path))
                    std::filesystem::remove(m_path);
                if (std::filesystem::exists(m_path.wstring() + L".tmp"))
                    std::filesystem::remove(m_path.wstring() + L".tmp");
            }
            catch (...)
            {
                // Ignore cleanup errors
            }
        }
    private:
        std::filesystem::path m_path;
    };
}

int RunThemeMigrationTests()
{
    // Test 1: Underscore IDs normalized to kebab-case
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);
        
        SettingsStore store;
        store.Load(tempPath);
        
        store.Set(L"appearance.theme.style", L"graphite_office");
        
        ThemeMigrationService migration(&store);
        if (!migration.Migrate())
            return Fail("Test 1: Migration should succeed");

        const std::wstring themeId = store.Get(L"theme.win32.theme_id", L"");
        if (themeId != L"graphite-office")
            return Fail("Test 1: theme.win32.theme_id should be graphite-office (kebab-case)");

        if (store.Get(L"theme.source", L"") != L"win32_theme_system")
            return Fail("Test 1: theme.source should be set to win32_theme_system");
    }

    // Test 2: Missing keys are populated correctly
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);
        
        SettingsStore store;
        store.Load(tempPath);
        // Leave all keys empty/unset to test backfill
        
        ThemeMigrationService migration(&store);
        if (!migration.Migrate())
            return Fail("Test 2: Migration should succeed with empty store");

        if (store.Get(L"theme.source", L"") != L"win32_theme_system")
            return Fail("Test 2: theme.source should be populated to win32_theme_system");

        const std::wstring themeId = store.Get(L"theme.win32.theme_id", L"");
        if (themeId != L"graphite-office")
            return Fail("Test 2: Missing theme ID should default to graphite-office");
    }

    // Test 3: Idempotency - second run does not rewrite correct values
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);
        
        SettingsStore store;
        store.Load(tempPath);
        
        store.Set(L"theme.win32.theme_id", L"graphite-office");
        store.Set(L"theme.win32.catalog_version", L"1.0.0");
        store.Set(L"theme.migration_v2_complete", L"true");
        
        ThemeMigrationService migration1(&store);
        if (!migration1.Migrate())
            return Fail("Test 3: First migration should succeed");

        // Second migration should be skipped due to marker.
        ThemeMigrationService migration2(&store);
        if (!migration2.Migrate())
            return Fail("Test 3: Second migration should be idempotent");
        
        // Verify values unchanged.
        if (store.Get(L"theme.win32.theme_id", L"") != L"graphite-office")
            return Fail("Test 3: theme ID should remain unchanged");
        if (store.Get(L"theme.win32.catalog_version", L"") != L"1.0.0")
            return Fail("Test 3: catalog version should remain unchanged");
    }

    // Test 4: Invalid theme ID falls back to graphite-office
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);
        
        SettingsStore store;
        store.Load(tempPath);
        
        store.Set(L"theme.win32.theme_id", L"invalid-unknown-theme");
        
        ThemeMigrationService migration(&store);
        if (!migration.Migrate())
            return Fail("Test 4: Migration should handle invalid theme gracefully");
        
        // Invalid theme should fallback to graphite-office
        const std::wstring themeId = store.Get(L"theme.win32.theme_id", L"");
        if (themeId != L"graphite-office")
            return Fail("Test 4: Invalid theme should fallback to graphite-office");
    }

    return 0;
}

int RunPluginConflictDetectionTests()
{
    // Placeholder for plugin conflict detection tests.
    // To be expanded in production.
    return 0;
}

int RunThemePackageValidationTests()
{
    // Placeholder for theme package validation tests.
    // To be expanded in production.
    return 0;
}
