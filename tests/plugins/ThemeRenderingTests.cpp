#include <string>
#include <iostream>
#include <cstdio>
#include <filesystem>
#include <ctime>

#include "core/PluginAppearanceConflictGuard.h"
#include "core/SettingsStore.h"
#include "core/ThemeApplyPipeline.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }

    std::filesystem::path GetUniqueTempPath()
    {
        static int counter = 0;
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        return tempDir / ("theme_rendering_test_" + std::to_string(counter++) + "_" + std::to_string(std::time(nullptr)) + ".json");
    }

    class TempFileGuard
    {
    public:
        explicit TempFileGuard(const std::filesystem::path& path)
            : m_path(path)
        {
        }

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
}

int RunThemeRenderingConsistencyTests()
{
    // Selector test: canonical plugin is active; legacy alias remains accepted.
    {
        if (!PluginAppearanceConflictGuard::IsCanonicalSelector(L"community.visual_modes"))
            return Fail("Selector test: community.visual_modes should be canonical selector");

        if (!PluginAppearanceConflictGuard::IsCanonicalSelector(L"builtin.appearance"))
            return Fail("Selector test: builtin.appearance should be accepted as compatibility alias");
    }

    // Conflict test: extra appearance plugins are blocked from theme-write paths.
    {
        PluginAppearanceConflictGuard guard;

        const bool nonAppearanceConflict = guard.HasAppearanceConflict(
            L"community.extra_widget",
            {L"widget.open", L"widget.pin"});
        if (nonAppearanceConflict)
            return Fail("Conflict test: non-appearance plugin commands should not be blocked");

        const bool conflict = guard.HasAppearanceConflict(
            L"community.theme_switcher_alt",
            {L"appearance.mode.dark", L"theme.apply"});
        if (!conflict)
            return Fail("Conflict test: extra appearance selector should be blocked");
    }

    return 0;
}

int RunThemeFallbackTests()
{
    // Fallback test: unknown theme ID gracefully falls back to graphite-office.
    {
        const std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);
        const auto result = pipeline.ApplyTheme(L"invalid-theme-xyz");
        if (!result.success)
            return Fail("Fallback test: apply should not fail for unknown theme");

        if (result.appliedThemeId != L"graphite-office")
            return Fail("Fallback test: unknown ID should fall back to graphite-office");

        if (result.fallbackReason.empty())
            return Fail("Fallback test: fallback reason should be provided");

        if (store.Get(L"theme.win32.theme_id", L"") != L"graphite-office")
            return Fail("Fallback test: persisted theme ID should be graphite-office");
    }

    return 0;
}

int RunThemePersistenceTests()
{
    // Persistence test: chosen theme survives restart unchanged.
    {
        const std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);
        const auto apply = pipeline.ApplyTheme(L"aurora-light");
        if (!apply.success)
            return Fail("Persistence test: applying aurora-light should succeed");

        SettingsStore reloaded;
        reloaded.Load(tempPath);
        if (reloaded.Get(L"theme.win32.theme_id", L"") != L"aurora-light")
            return Fail("Persistence test: theme ID should survive restart");

        if (reloaded.Get(L"theme.source", L"") != L"win32_theme_system")
            return Fail("Persistence test: theme.source should survive restart");

        if (reloaded.Get(L"theme.preset", L"") != L"aurora-light")
            return Fail("Persistence test: theme.preset bridge key should survive restart");

        if (reloaded.Get(L"theme.win32.display_name", L"") != L"Aurora Light")
            return Fail("Persistence test: display name should survive restart");

        if (reloaded.Get(L"theme.win32.catalog_version", L"") != L"2026.04.06")
            return Fail("Persistence test: catalog version should survive restart");
    }

    // Persistence test: normalization is persisted for compatibility aliases.
    {
        const std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);
        const auto apply = pipeline.ApplyTheme(L"AURORA_LIGHT");
        if (!apply.success)
            return Fail("Persistence test: applying normalized alias should succeed");

        if (apply.appliedThemeId != L"aurora-light")
            return Fail("Persistence test: alias should normalize to aurora-light");

        if (store.Get(L"theme.win32.theme_id", L"") != L"aurora-light")
            return Fail("Persistence test: normalized ID should persist");

        if (store.Get(L"theme.preset", L"") != L"aurora-light")
            return Fail("Persistence test: normalized bridge key should persist");
    }

    // Performance test: rapid theme switch requests are debounced/coalesced.
    {
        const std::filesystem::path tempPath = GetUniqueTempPath();
        TempFileGuard guard(tempPath);

        SettingsStore store;
        store.Load(tempPath);

        ThemeApplyPipeline pipeline(&store);
        const auto first = pipeline.ApplyTheme(L"aurora-light");
        if (!first.success)
            return Fail("Performance test: first apply should succeed");

        const auto second = pipeline.ApplyTheme(L"nocturne-dark");
        if (!second.success)
            return Fail("Performance test: debounced second apply should still report success");

        if (second.appliedThemeId != L"aurora-light")
            return Fail("Performance test: rapid switch should be coalesced to current active theme");

        if (second.fallbackReason.find(L"Debounced") == std::wstring::npos)
            return Fail("Performance test: debounced apply should include debounce reason");

        if (store.Get(L"theme.win32.theme_id", L"") != L"aurora-light")
            return Fail("Performance test: debounced request should not overwrite persisted theme");
    }

    return 0;
}
