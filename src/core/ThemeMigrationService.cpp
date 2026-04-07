#include "core/ThemeMigrationService.h"

#include "core/SettingsStore.h"

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

    return source == L"win32_theme_system" && IsValidThemeId(themeId) && !displayName.empty();
}

bool ThemeMigrationService::PerformMigration()
{
    // Ensure theme.source is set to canonical value.
    const std::wstring source = m_settingsStore->Get(L"theme.source", L"");
    if (source != L"win32_theme_system")
    {
        m_settingsStore->Set(L"theme.source", L"win32_theme_system");
    }

    // Derive/migrate theme ID from legacy keys or existing canonical key.
    std::wstring themeId = m_settingsStore->Get(L"theme.win32.theme_id", L"");

    if (themeId.empty())
    {
        // Check legacy appearance.theme.style key.
        const std::wstring legacyStyle = m_settingsStore->Get(L"appearance.theme.style", L"");
        if (!legacyStyle.empty())
        {
            themeId = NormalizeThemeIdToKebabCase(legacyStyle);
        }
    }

    // Validate and fallback if necessary.
    if (!IsValidThemeId(themeId))
    {
        themeId = GetFallbackThemeId();
    }

    // Persist canonical theme ID.
    m_settingsStore->Set(L"theme.win32.theme_id", themeId);

    // Populate display name from theme ID (human-readable form).
    std::wstring displayName = m_settingsStore->Get(L"theme.win32.display_name", L"");
    if (displayName.empty())
    {
        // Simple conversion: kebab-case → Title Case
        // For now, just capitalize first letter and show title form.
        // In production, this would look up from a catalog.
        if (themeId == L"graphite-office")
            displayName = L"Graphite Office";
        else if (themeId == L"amber-terminal")
            displayName = L"Amber Terminal";
        else if (themeId == L"arctic-glass")
            displayName = L"Arctic Glass";
        // ... etc. For full implementation, use a theme metadata catalog.
        else
            displayName = themeId; // Fallback to ID itself.

        m_settingsStore->Set(L"theme.win32.display_name", displayName);
    }

    // Populate catalog version.
    std::wstring catalogVersion = m_settingsStore->Get(L"theme.win32.catalog_version", L"");
    if (catalogVersion.empty())
    {
        m_settingsStore->Set(L"theme.win32.catalog_version", L"1.0.0");
    }

    // Ensure theme.mode is set (light/dark).
    std::wstring mode = m_settingsStore->Get(L"theme.mode", L"");
    if (mode.empty())
    {
        // Check legacy key.
        const std::wstring legacyMode = m_settingsStore->Get(L"appearance.theme.mode", L"");
        if (legacyMode == L"dark" || legacyMode == L"light")
        {
            m_settingsStore->Set(L"theme.mode", legacyMode);
        }
        else
        {
            // Default to light if unset.
            m_settingsStore->Set(L"theme.mode", L"light");
        }
    }

    // Persist all changes immediately so they're not lost.
    return m_settingsStore->Save();
}

bool ThemeMigrationService::PersistMigrationMarker()
{
    m_settingsStore->Set(L"theme.migration_v2_complete", L"true");
    return m_settingsStore->Save();
}
