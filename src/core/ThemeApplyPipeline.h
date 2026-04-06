#pragma once

#include <string>
#include <vector>

class SettingsStore;
struct ThemePalette;

/// Atomic theme application pipeline with fallback safety.
/// Ensures theme changes don't leave the UI in a partially-applied state.
class ThemeApplyPipeline
{
public:
    explicit ThemeApplyPipeline(SettingsStore* settingsStore);
    ~ThemeApplyPipeline() = default;

    /// Result of theme application attempt
    struct ApplyResult
    {
        bool success = false;
        std::wstring errorMessage;
        std::wstring appliedThemeId;
        std::wstring fallbackReason;  // If fallback was used

        static ApplyResult Success(const std::wstring& themeId)
        {
            return {true, L"", themeId, L""};
        }

        static ApplyResult SuccessWithFallback(const std::wstring& themeId, const std::wstring& reason)
        {
            return {true, L"", themeId, reason};
        }

        static ApplyResult Failure(const std::wstring& error)
        {
            return {false, error, L"", L""};
        }
    };

    /// Apply a theme by ID with atomic palette update and persistence.
    /// Falls back to graphite-office if theme not found.
    ApplyResult ApplyTheme(const std::wstring& themeId);

    /// Apply a custom palette directly (for third-party themes or testing).
    ApplyResult ApplyPalette(const ThemePalette& palette, const std::wstring& themeId);

    /// Get the currently applied theme ID.
    std::wstring GetCurrentThemeId() const;

    /// Validate theme ID exists in catalog.
    bool ValidateThemeId(const std::wstring& themeId) const;

private:
    SettingsStore* m_settingsStore = nullptr;

    /// Persist theme selection to settings store (atomic operation).
    bool PersistThemeSelection(const std::wstring& themeId);

    /// Get known theme catalog IDs (graphite-office, aurora-light, etc.)
    static const std::vector<std::wstring>& GetKnownThemeIds();

    /// Build palette for a given known theme ID.
    ThemePalette BuildKnownThemePalette(const std::wstring& themeId) const;
};
