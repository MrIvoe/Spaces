#include "core/PluginAppearanceConflictGuard.h"

#include <algorithm>
#include <iostream>

bool PluginAppearanceConflictGuard::HasAppearanceConflict(
    const std::wstring& pluginId,
    const std::vector<std::wstring>& commandIds) const
{
    // If this is the canonical selector itself, no conflict.
    if (IsCanonicalSelector(pluginId))
        return false;

    // Check if any command ID matches appearance patterns.
    for (const auto& cmd : commandIds)
    {
        if (IsAppearanceCommandPattern(cmd))
        {
            std::wcerr << L"[Plugin Conflict] Plugin '" << pluginId
                       << L"' declares appearance command '" << cmd
                       << L"' but '" << GetCanonicalAppearanceSelectorId()
                       << L"' is the active selector. Disabling conflicting path.\n";
            return true;
        }
    }

    return false;
}

std::wstring PluginAppearanceConflictGuard::GetCanonicalAppearanceSelectorId()
{
    return L"community.visual_modes";
}

bool PluginAppearanceConflictGuard::IsCanonicalSelector(const std::wstring& pluginId)
{
    if (pluginId == GetCanonicalAppearanceSelectorId())
        return true;

    // Compatibility alias while built-in selector manifest id migrates.
    return pluginId == L"builtin.appearance";
}

bool PluginAppearanceConflictGuard::IsAppearanceCommandPattern(const std::wstring& commandId)
{
    // Patterns that indicate appearance/theme control:
    // - appearance.*
    // - theme.*
    // - visual.*
    // - color.*
    // - legacy command IDs like organizer.appearance.mode

    if (commandId.find(L"appearance.") == 0)
        return true;
    if (commandId.find(L"theme.") == 0)
        return true;
    if (commandId.find(L"visual.") == 0)
        return true;
    if (commandId.find(L"color.") == 0)
        return true;
    if (commandId.find(L".appearance.") != std::wstring::npos)
        return true;

    return false;
}

