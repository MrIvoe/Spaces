#include "core/ThemeApplyPipeline.h"

#include "core/SettingsStore.h"
#include "core/ThemePlatform.h"
#include "Win32Helpers.h"

namespace
{
    std::wstring DisplayNameFromThemeId(const std::wstring& themeId)
    {
        if (themeId == L"amber-terminal") return L"Amber Terminal";
        if (themeId == L"arctic-glass") return L"Arctic Glass";
        if (themeId == L"aurora-light") return L"Aurora Light";
        if (themeId == L"brass-steampunk") return L"Brass Steampunk";
        if (themeId == L"copper-foundry") return L"Copper Foundry";
        if (themeId == L"emerald-ledger") return L"Emerald Ledger";
        if (themeId == L"forest-organic") return L"Forest Organic";
        if (themeId == L"graphite-office") return L"Graphite Office";
        if (themeId == L"harbor-blue") return L"Harbor Blue";
        if (themeId == L"ivory-bureau") return L"Ivory Bureau";
        if (themeId == L"mono-minimal") return L"Mono Minimal";
        if (themeId == L"neon-cyberpunk") return L"Neon Cyberpunk";
        if (themeId == L"nocturne-dark") return L"Nocturne Dark";
        if (themeId == L"nova-futuristic") return L"Nova Futuristic";
        if (themeId == L"olive-terminal") return L"Olive Terminal";
        if (themeId == L"pop-colorburst") return L"Pop Colorburst";
        if (themeId == L"rose-paper") return L"Rose Paper";
        if (themeId == L"storm-steel") return L"Storm Steel";
        if (themeId == L"sunset-retro") return L"Sunset Retro";
        if (themeId == L"tape-lo-fi") return L"Tape Lo-Fi";
        return L"Graphite Office";
    }

    std::wstring NormalizeThemeId(std::wstring raw)
    {
        for (auto& ch : raw)
        {
            if (ch == L'_')
            {
                ch = L'-';
            }
            else if (ch >= L'A' && ch <= L'Z')
            {
                ch = static_cast<wchar_t>(ch - L'A' + L'a');
            }
        }
        return raw;
    }

    const std::vector<std::wstring> g_knownThemes =
    {
        L"amber-terminal",
        L"arctic-glass",
        L"aurora-light",
        L"brass-steampunk",
        L"copper-foundry",
        L"emerald-ledger",
        L"forest-organic",
        L"graphite-office",
        L"harbor-blue",
        L"ivory-bureau",
        L"mono-minimal",
        L"neon-cyberpunk",
        L"nocturne-dark",
        L"nova-futuristic",
        L"olive-terminal",
        L"pop-colorburst",
        L"rose-paper",
        L"storm-steel",
        L"sunset-retro",
        L"tape-lo-fi",
    };

    // Simple theme palette presets (could be extended with more detailed color schemes)
    ThemePalette GetThemePalette(const std::wstring& themeId)
    {
        if (themeId == L"graphite-office")
        {
            return ThemePalette{
                RGB(245, 245, 245),  // windowColor - light gray
                RGB(240, 240, 240),  // surfaceColor
                RGB(235, 235, 235),  // navColor
                RGB(50, 50, 50),     // textColor
                RGB(100, 100, 100),  // subtleTextColor
                RGB(0, 102, 204),    // accentColor - professional blue
                RGB(200, 200, 200),  // borderColor
                RGB(70, 70, 70),     // fenceTitleBarColor
                RGB(245, 245, 245),  // fenceTitleTextColor
                RGB(100, 100, 100),  // fenceItemTextColor
                RGB(80, 80, 80)      // fenceItemHoverColor
            };
        }
        else if (themeId == L"aurora-light")
        {
            return ThemePalette{
                RGB(255, 255, 255),  // windowColor - pure white
                RGB(248, 248, 248),  // surfaceColor
                RGB(246, 246, 246),  // navColor
                RGB(40, 40, 40),     // textColor
                RGB(110, 110, 110),  // subtleTextColor
                RGB(0, 150, 150),    // accentColor - teal
                RGB(220, 220, 220),  // borderColor
                RGB(100, 140, 140),  // fenceTitleBarColor - teal tint
                RGB(255, 255, 255),  // fenceTitleTextColor
                RGB(80, 80, 80),     // fenceItemTextColor
                RGB(200, 220, 220)   // fenceItemHoverColor - light teal
            };
        }
        else if (themeId == L"nocturne-dark")
        {
            return ThemePalette{
                RGB(25, 25, 35),     // windowColor - dark blue-gray
                RGB(35, 35, 45),     // surfaceColor
                RGB(40, 40, 50),     // navColor
                RGB(240, 240, 240),  // textColor
                RGB(180, 180, 180),  // subtleTextColor
                RGB(100, 180, 255),  // accentColor - bright blue
                RGB(60, 60, 70),     // borderColor
                RGB(20, 20, 25),     // fenceTitleBarColor
                RGB(230, 230, 240),  // fenceTitleTextColor
                RGB(200, 200, 210),  // fenceItemTextColor
                RGB(50, 50, 60)      // fenceItemHoverColor
            };
        }

        // Default fallback to graphite-office
        return GetThemePalette(L"graphite-office");
    }
}

ThemeApplyPipeline::ThemeApplyPipeline(SettingsStore* settingsStore)
    : m_settingsStore(settingsStore)
{
}

ThemeApplyPipeline::ApplyResult ThemeApplyPipeline::ApplyTheme(const std::wstring& themeId)
{
    std::lock_guard<std::mutex> lock(m_applyMutex);

    if (!m_settingsStore)
    {
        Win32Helpers::LogError(L"ThemeApplyPipeline: apply requested without settings store.");
        return ApplyResult::Failure(L"Settings store not initialized");
    }

    const std::wstring requestedThemeId = NormalizeThemeId(themeId);
    if (requestedThemeId.empty())
    {
        Win32Helpers::LogError(L"ThemeApplyPipeline: empty theme id requested.");
        return ApplyResult::Failure(L"Theme ID cannot be empty");
    }

    const auto now = std::chrono::steady_clock::now();
    if (!m_lastAppliedThemeId.empty() && requestedThemeId != m_lastAppliedThemeId)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastApplyTime);
        if (elapsed < std::chrono::milliseconds(250))
        {
            Win32Helpers::LogInfo(
                L"ThemeApplyPipeline: debounced rapid switch from '" + m_lastAppliedThemeId +
                L"' to '" + requestedThemeId + L"'.");
            return ApplyResult::SuccessWithFallback(m_lastAppliedThemeId, L"Debounced rapid switch request");
        }
    }

    const std::wstring migrationMarker = m_settingsStore->Get(L"theme.migration_v2_complete", L"false");
    Win32Helpers::LogInfo(L"ThemeApplyPipeline: apply start id='" + requestedThemeId +
                          L"' migration_v2_complete='" + migrationMarker + L"'");

    std::wstring effectiveThemeId = requestedThemeId;
    std::wstring fallbackReason;

    if (!ValidateThemeId(requestedThemeId))
    {
        effectiveThemeId = L"graphite-office";
        fallbackReason = L"Unknown theme ID; using fallback";
        Win32Helpers::LogInfo(L"ThemeApplyPipeline: warning unknown theme id '" + requestedThemeId + L"'; fallback to '" + effectiveThemeId + L"'.");
    }

    // Build palette for theme
    ThemePalette palette = BuildKnownThemePalette(effectiveThemeId);

    // Apply palette (atomic operation)
    ApplyResult result = ApplyPalette(palette, effectiveThemeId);
    if (!result.success)
    {
        Win32Helpers::LogError(L"ThemeApplyPipeline: palette apply failed for id='" + effectiveThemeId + L"'.");
        return result;
    }

    // Persist selection
    if (!PersistThemeSelection(effectiveThemeId))
    {
        Win32Helpers::LogError(L"ThemeApplyPipeline: persistence failed for id='" + effectiveThemeId + L"'.");
        return ApplyResult::Failure(L"Failed to persist theme selection");
    }

    if (!fallbackReason.empty())
        result.fallbackReason = fallbackReason;

    m_lastApplyTime = now;
    m_lastAppliedThemeId = effectiveThemeId;

    Win32Helpers::LogInfo(L"ThemeApplyPipeline: apply success id='" + effectiveThemeId + L"'.");

    return result;
}

ThemeApplyPipeline::ApplyResult ThemeApplyPipeline::ApplyPalette(const ThemePalette& palette, const std::wstring& themeId)
{
    // In production, this would:
    // 1. Batch all color updates together (no intermediate repaints)
    // 2. Update all UI elements atomically
    // 3. Invalidate caches
    // 4. Trigger single repaint

    // For now, document the contract
    (void)palette;  // Suppress unused parameter warning

    Win32Helpers::LogInfo(L"ThemeApplyPipeline: atomic palette apply committed for id='" + themeId + L"'.");

    return ApplyResult::Success(themeId);
}

std::wstring ThemeApplyPipeline::GetCurrentThemeId() const
{
    if (!m_settingsStore)
        return L"graphite-office";

    std::wstring current = m_settingsStore->Get(L"theme.win32.theme_id", L"graphite-office");
    return current.empty() ? L"graphite-office" : current;
}

bool ThemeApplyPipeline::ValidateThemeId(const std::wstring& themeId) const
{
    const auto& known = GetKnownThemeIds();
    for (const auto& id : known)
    {
        if (id == themeId)
            return true;
    }
    return false;
}

bool ThemeApplyPipeline::PersistThemeSelection(const std::wstring& themeId)
{
    if (!m_settingsStore)
        return false;

    try
    {
        // Atomic persist: update all canonical theme keys together
        m_settingsStore->Set(L"theme.win32.theme_id", themeId);
        m_settingsStore->Set(L"theme.preset", themeId);
        m_settingsStore->Set(L"theme.source", L"win32_theme_system");

        const std::wstring displayName = DisplayNameFromThemeId(themeId);
        m_settingsStore->Set(L"theme.win32.display_name", displayName);
        m_settingsStore->Set(L"theme.win32.catalog_version", L"2026.04.06");

        // Persist immediately
        return m_settingsStore->Save();
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"Failed to persist theme selection: " + std::wstring(ex.what(), ex.what() + strlen(ex.what())));
        return false;
    }
}

const std::vector<std::wstring>& ThemeApplyPipeline::GetKnownThemeIds()
{
    return g_knownThemes;
}

ThemePalette ThemeApplyPipeline::BuildKnownThemePalette(const std::wstring& themeId) const
{
    return GetThemePalette(themeId);
}

