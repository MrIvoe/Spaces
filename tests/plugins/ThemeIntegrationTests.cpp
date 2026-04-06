#include <string>
#include <iostream>
#include <filesystem>
#include <ctime>
#include <cstdlib>

#include "core/ThemeApplyPipeline.h"
#include "core/ThemePackageLoader.h"
#include "core/ThemePackageValidator.h"
#include "core/ThemePlatform.h"
#include "core/ThemeTokenResolver.h"
#include "core/SettingsStore.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }

    // Generate unique temp file path
    std::filesystem::path GetUniqueTempPath()
    {
        static int counter = 0;
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        return tempDir / ("integration_test_" + std::to_string(counter++) + "_" + std::to_string(std::time(nullptr)) + ".json");
    }

    // RAII cleanup
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
            }
        }
    private:
        std::filesystem::path m_path;
    };

    std::filesystem::path FindRepoRoot()
    {
        std::filesystem::path current = std::filesystem::current_path();
        for (int i = 0; i < 8; ++i)
        {
            if (std::filesystem::exists(current / "CMakeLists.txt") &&
                std::filesystem::exists(current / "tests"))
            {
                return current;
            }

            if (!current.has_parent_path())
                break;
            current = current.parent_path();
        }

        return {};
    }

    std::string EscapePowerShellSingleQuoted(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8);
        for (char c : input)
        {
            if (c == '\'')
            {
                out += "''";
            }
            else
            {
                out.push_back(c);
            }
        }
        return out;
    }

    bool CreateZipFromDirectory(const std::filesystem::path& sourceDir, const std::filesystem::path& zipPath)
    {
        std::error_code ec;
        std::filesystem::remove(zipPath, ec);

        const std::string source = EscapePowerShellSingleQuoted(sourceDir.string());
        const std::string dest = EscapePowerShellSingleQuoted(zipPath.string());
        const std::string command =
            "powershell -NoProfile -Command \"Compress-Archive -Path '" + source + "\\*' -DestinationPath '" + dest + "' -Force\"";

        return std::system(command.c_str()) == 0 && std::filesystem::exists(zipPath);
    }
}

int RunThemeApplyIntegrationTests()
{
    // Test 1: Apply valid theme and persist to settings
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);

        // Apply a known theme
        auto result = pipeline.ApplyTheme(L"aurora-light");
        if (!result.success)
            return Fail("Test 1: Should successfully apply aurora-light theme");

        if (result.appliedThemeId != L"aurora-light")
            return Fail("Test 1: Should report applied theme ID");

        // Verify persistence
        if (store.Get(L"theme.win32.theme_id", L"") != L"aurora-light")
            return Fail("Test 1: Theme ID should be persisted");

        if (store.Get(L"theme.source", L"") != L"win32_theme_system")
            return Fail("Test 1: Theme source should be set");
    }

    // Test 2: Unknown theme falls back to graphite-office
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);

        // Apply invalid theme
        auto result = pipeline.ApplyTheme(L"invalid-theme-xyz");
        if (!result.success)
            return Fail("Test 2: Should gracefully handle invalid theme");

        // Should fall back to graphite-office
        if (result.appliedThemeId != L"graphite-office")
            return Fail("Test 2: Should fall back to graphite-office for unknown theme");

        if (result.fallbackReason.empty())
            return Fail("Test 2: Should report fallback reason");
    }

    // Test 3: Validate theme ID function
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);

        // Valid theme IDs
        if (!pipeline.ValidateThemeId(L"graphite-office"))
            return Fail("Test 3: Should recognize graphite-office as valid");

        if (!pipeline.ValidateThemeId(L"aurora-light"))
            return Fail("Test 3: Should recognize aurora-light as valid");

        if (!pipeline.ValidateThemeId(L"nocturne-dark"))
            return Fail("Test 3: Should recognize nocturne-dark as valid");

        // Invalid theme IDs
        if (pipeline.ValidateThemeId(L"invalid-theme"))
            return Fail("Test 3: Should reject invalid-theme");

        if (pipeline.ValidateThemeId(L""))
            return Fail("Test 3: Should reject empty theme ID");
    }

    return 0;
}

int RunThemeTokenResolverIntegrationTests()
{
    // Test 1: Hex color parsing
    {
        COLORREF white = ThemeTokenResolver::HexToColorRef(L"#FFFFFF");
        if (white != RGB(255, 255, 255))
            return Fail("Test TokenResolver 1: Should parse white correctly");

        COLORREF black = ThemeTokenResolver::HexToColorRef(L"#000000");
        if (black != RGB(0, 0, 0))
            return Fail("Test TokenResolver 1: Should parse black correctly");

        COLORREF red = ThemeTokenResolver::HexToColorRef(L"#FF0000");
        if (red != RGB(255, 0, 0))
            return Fail("Test TokenResolver 1: Should parse red correctly");
    }

    // Test 2: Invalid hex colors
    {
        COLORREF invalid = ThemeTokenResolver::HexToColorRef(L"#GGGGGG");
        if (invalid != static_cast<COLORREF>(-1))
            return Fail("Test TokenResolver 2: Should reject invalid hex");

        COLORREF tooshort = ThemeTokenResolver::HexToColorRef(L"#FFF");
        if (tooshort != static_cast<COLORREF>(-1))
            return Fail("Test TokenResolver 2: Should reject short hex");
    }

    // Test 3: Token map building
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeTokenResolver resolver(&store);

        // Create a token map
        std::unordered_map<std::wstring, std::wstring> tokens;
        tokens[L"win32.base.window_color"] = L"#FFFFFF";
        tokens[L"win32.base.text_color"] = L"#000000";
        tokens[L"win32.fence.title_bar_color"] = L"#FF0000";

        ThemePalette palette = resolver.BuildPaletteFromTokens(tokens);

        if (palette.windowColor != RGB(255, 255, 255))
            return Fail("Test TokenResolver 3: Should resolve window_color token");

        if (palette.textColor != RGB(0, 0, 0))
            return Fail("Test TokenResolver 3: Should resolve text_color token");

        if (palette.fenceTitleBarColor != RGB(255, 0, 0))
            return Fail("Test TokenResolver 3: Should resolve fence title_bar_color token");
    }

    return 0;
}

int RunThemeFullLifecycleTests()
{
    // Comprehensive test: Migration → Apply → Load → Switch
    {
        std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        // Set up legacy settings (simulate upgrade scenario)
        store.Set(L"appearance.theme.style", L"graphite_office");

        // Simulate migration
        store.Set(L"theme.source", L"win32_theme_system");
        store.Set(L"theme.win32.theme_id", L"graphite-office");
        store.Set(L"theme.migration_v2_complete", L"true");

        // Apply a different theme
        ThemeApplyPipeline pipeline(&store);
        auto result = pipeline.ApplyTheme(L"aurora-light");
        if (!result.success)
            return Fail("Test Lifecycle: Should apply theme after migration");

        // Simulate app restart: reload and verify theme is still applied
        SettingsStore store2;
        store2.Load(tempPath);

        std::wstring currentTheme = store2.Get(L"theme.win32.theme_id", L"");
        if (currentTheme != L"aurora-light")
            return Fail("Test Lifecycle: Theme should persist across restart");

        // Verify migration marker is set (no re-migration)
        if (store2.Get(L"theme.migration_v2_complete", L"") != L"true")
            return Fail("Test Lifecycle: Migration marker should persist");
    }

    return 0;
}

int RunThemePackageValidationIntegrationTests()
{
    const std::filesystem::path repoRoot = FindRepoRoot();
    if (repoRoot.empty())
    {
        return Fail("Theme package tests: failed to locate repo root");
    }

    const std::filesystem::path fixtureRoot = repoRoot / "tests" / "fixtures" / "theme_packages";
    if (!std::filesystem::exists(fixtureRoot))
    {
        return Fail("Theme package tests: fixture folder missing");
    }

    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "SimpleFencesThemeFixtureZips";
    std::filesystem::create_directories(tempDir);

    // Test 1: valid package accepted by validator and loader
    {
        const std::filesystem::path src = fixtureRoot / "valid_minimal";
        const std::filesystem::path zip = tempDir / "valid_minimal.zip";
        if (!CreateZipFromDirectory(src, zip))
            return Fail("Theme package tests: failed to create valid fixture zip");

        ThemePackageValidator validator;
        const auto validation = validator.ValidatePackage(zip.wstring());
        if (!validation.isValid)
            return Fail("Theme package tests: validator rejected valid package");

        if (validation.themeId != L"sample-valid-theme")
            return Fail("Theme package tests: wrong theme id parsed from valid package");

        ThemePackageLoader loader;
        const auto load = loader.LoadPackage(zip.wstring());
        if (!load.success)
            return Fail("Theme package tests: loader failed valid package");

        if (load.tokenMap.tokens.empty())
            return Fail("Theme package tests: expected tokens in valid package");

        ThemePackageLoader::CleanupExtraction(load.extractedPath);
    }

    // Test 2: package missing metadata is rejected
    {
        const std::filesystem::path src = fixtureRoot / "invalid_missing_metadata";
        const std::filesystem::path zip = tempDir / "invalid_missing_metadata.zip";
        if (!CreateZipFromDirectory(src, zip))
            return Fail("Theme package tests: failed to create missing-metadata fixture zip");

        ThemePackageValidator validator;
        const auto validation = validator.ValidatePackage(zip.wstring());
        if (validation.isValid)
            return Fail("Theme package tests: expected missing metadata package to be rejected");
    }

    // Test 3: package with forbidden file is rejected
    {
        const std::filesystem::path src = fixtureRoot / "invalid_forbidden_content";
        const std::filesystem::path zip = tempDir / "invalid_forbidden_content.zip";
        if (!CreateZipFromDirectory(src, zip))
            return Fail("Theme package tests: failed to create forbidden-content fixture zip");

        ThemePackageValidator validator;
        const auto validation = validator.ValidatePackage(zip.wstring());
        if (validation.isValid)
            return Fail("Theme package tests: expected forbidden-content package to be rejected");
    }

    return 0;
}
