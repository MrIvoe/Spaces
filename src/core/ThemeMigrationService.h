#pragma once

#include <string>

class SettingsStore;

/// Manages idempotent migration from legacy theme settings to Win32ThemeSystem canonical format.
/// Runs once on app startup before any theme rendering.
class ThemeMigrationService
{
public:
    explicit ThemeMigrationService(SettingsStore* settingsStore = nullptr);

    ~ThemeMigrationService() = default;

    /// Run migration if not already completed. Idempotent.
    /// Returns true if migration succeeded or was skipped (already complete).
    /// Returns false if an error occurred.
    bool Migrate();

    /// Check if migration has already been completed.
    bool IsMigrationComplete() const;

private:
    SettingsStore* m_settingsStore = nullptr;

    /// Convert underscore-separated theme names to kebab-case (graphite_office -> graphite-office).
    static std::wstring NormalizeThemeIdToKebabCase(const std::wstring& rawId);

    /// Check if a theme ID is a valid known theme in Win32ThemeCatalog.
    static bool IsValidThemeId(const std::wstring& kebabId);

    /// Get fallback theme ID when none is known/valid.
    static std::wstring GetFallbackThemeId();

    /// Phase 1: Check if already migrated.
    bool IsMigrationMarkerSet() const;

    /// Phase 2-3: Perform migration steps (normalize, populate, validate).
    bool PerformMigration();

    /// Phase 4: Persist migration marker and all changes.
    bool PersistMigrationMarker();
};
