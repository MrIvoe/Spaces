using IVOEFences.Core.Plugins;
using IVOEFences.Shell.Fences;

namespace IVOEFences.Shell.Settings;

/// <summary>
/// Transitional settings host entrypoint. It establishes the future page-based
/// architecture while delegating to the existing SettingsWindow implementation
/// until all tabs are migrated.
/// </summary>
internal static class SettingsWindowHost
{
    public static void ShowOrFocus(FenceManager fenceManager, IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        SettingsWindow.ShowOrFocus(fenceManager, pluginSettings);
    }
}
