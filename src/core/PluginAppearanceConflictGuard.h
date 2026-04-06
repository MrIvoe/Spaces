#pragma once

#include <string>
#include <vector>

class SettingsStore;

/// Detects and prevents conflicting appearance-selector plugins.
/// Only community.visual_modes may write to theme settings.
class PluginAppearanceConflictGuard
{
public:
    explicit PluginAppearanceConflictGuard() = default;

    ~PluginAppearanceConflictGuard() = default;

    /// Check if plugin declares appearance-related command IDs that conflict with
    /// the designated community.visual_modes plugin. If conflict is detected,
    /// logs a warning and returns true. Plugin loader should disable conflicting paths.
    bool HasAppearanceConflict(const std::wstring& pluginId, const std::vector<std::wstring>& commandIds) const;

    /// Get the canonical appearance selector plugin ID.
    static std::wstring GetCanonicalAppearanceSelectorId();

    /// Check if the given plugin ID is the canonical selector.
    static bool IsCanonicalSelector(const std::wstring& pluginId);

private:
    /// Check if a command ID pattern matches appearance-related selectors.
    static bool IsAppearanceCommandPattern(const std::wstring& commandId);
};
