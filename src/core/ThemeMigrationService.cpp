#include "core/ThemeMigrationService.h"

#include "core/SettingsStore.h"

#include "Win32Helpers.h"

#include <algorithm>
#include <cctype>
#include <cwctype>

namespace
{
    // Built-in Win32ThemeCatalog theme IDs (always valid).
    constexpr const wchar_t* kKnownThemeIds[] =
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

    constexpr size_t kKnownThemeCount = sizeof(kKnownThemeIds) / sizeof(kKnownThemeIds[0]);

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
}

ThemeMigrationService::ThemeMigrationService(SettingsStore* settingsStore)
    : m_settingsStore(settingsStore)
{
}

bool ThemeMigrationService::Migrate()
{
    if (!m_settingsStore)
        return false;

    // Phase 1: Check idempotency marker.
    if (IsMigrationMarkerSet())
        return true; // Already migrated, skip.

    // Phase 2-3: Perform migration.
    if (!PerformMigration())
        return false;

    // Phase 4: Persist migration marker.
    return PersistMigrationMarker();
}

bool ThemeMigrationService::IsMigrationComplete() const
{
    if (!m_settingsStore)
        return false;

    const std::wstring marker = m_settingsStore->Get(L"theme.migration_v2_complete", L"");
    return marker == L"true";
}

std::wstring ThemeMigrationService::NormalizeThemeIdToKebabCase(const std::wstring& rawId)
{
    std::wstring normalized;
    normalized.reserve(rawId.size());

    for (wchar_t c : rawId)
    {
        if (iswspace(c) || c == L'_')
        {
            normalized.push_back(L'-');
        }
        else if (iswupper(c))
        {
            normalized.push_back(static_cast<wchar_t>(towlower(c)));
        }
        else
        {
            normalized.push_back(c);
        }
    }

    return normalized;
}

bool ThemeMigrationService::IsValidThemeId(const std::wstring& kebabId)
{
    for (size_t i = 0; i < kKnownThemeCount; ++i)
    {
        if (kebabId == kKnownThemeIds[i])
            return true;
    }
    return false;
}

std::wstring ThemeMigrationService::GetFallbackThemeId()
{
    return L"graphite-office";
}

bool ThemeMigrationService::IsMigrationMarkerSet() const
{
    const std::wstring marker = m_settingsStore->Get(L"theme.migration_v2_complete", L"");
    if (marker != L"true")
    {
        return false;
    }

    // Self-heal: if required canonical keys are missing/corrupt, rerun migration
    // even if a legacy marker was previously set.
    const std::wstring source = m_settingsStore->Get(L"theme.source", L"");
    const std::wstring themeId = NormalizeThemeIdToKebabCase(m_settingsStore->Get(L"theme.win32.theme_id", L""));
    const std::wstring displayName = m_settingsStore->Get(L"theme.win32.display_name", L"");
    const std::wstring catalogVersion = m_settingsStore->Get(L"theme.win32.catalog_version", L"");

    return source == L"win32_theme_system" && IsValidThemeId(themeId) && !displayName.empty() && !catalogVersion.empty();
}

bool ThemeMigrationService::PerformMigration()
{
    const std::wstring source = m_settingsStore->Get(L"theme.source", L"");
    if (source != L"win32_theme_system")
    {
        Win32Helpers::LogInfo(L"Theme migration: normalizing theme.source to win32_theme_system.");
        m_settingsStore->Set(L"theme.source", L"win32_theme_system");
    }

    // Derive from canonical key first, then legacy bridge keys.
    std::wstring themeId = m_settingsStore->Get(L"theme.win32.theme_id", L"");
    if (themeId.empty())
    {
        themeId = m_settingsStore->Get(L"theme.preset", L"");
    }
    if (themeId.empty())
    {
        themeId = m_settingsStore->Get(L"appearance.theme.style", L"");
    }

    const std::wstring originalThemeId = themeId;
    themeId = NormalizeThemeIdToKebabCase(themeId);

    if (!IsValidThemeId(themeId))
    {
        Win32Helpers::LogInfo(L"Theme migration: unknown theme id '" + originalThemeId + L"'; using graphite-office fallback.");
        themeId = GetFallbackThemeId();
    }

    m_settingsStore->Set(L"theme.win32.theme_id", themeId);
    // Compatibility bridge key kept for older plugins/routes.
    m_settingsStore->Set(L"theme.preset", themeId);

    std::wstring displayName = m_settingsStore->Get(L"theme.win32.display_name", L"");
    if (displayName.empty())
    {
        displayName = DisplayNameFromThemeId(themeId);
        m_settingsStore->Set(L"theme.win32.display_name", displayName);
    }

    std::wstring catalogVersion = m_settingsStore->Get(L"theme.win32.catalog_version", L"");
    if (catalogVersion.empty())
    {
        m_settingsStore->Set(L"theme.win32.catalog_version", L"2026.04.06");
    }

    std::wstring mode = m_settingsStore->Get(L"theme.mode", L"");
    if (mode.empty())
    {
        const std::wstring legacyMode = m_settingsStore->Get(L"appearance.theme.mode", L"");
        if (legacyMode == L"dark" || legacyMode == L"light")
        {
            m_settingsStore->Set(L"theme.mode", legacyMode);
        }
        else
        {
            m_settingsStore->Set(L"theme.mode", L"light");
        }
    }

    return m_settingsStore->Save();
}

bool ThemeMigrationService::PersistMigrationMarker()
{
    m_settingsStore->Set(L"theme.migration_v2_complete", L"true");
    return m_settingsStore->Save();
}

