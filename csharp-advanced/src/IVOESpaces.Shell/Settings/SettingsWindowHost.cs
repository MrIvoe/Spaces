using IVOESpaces.Core.Plugins;
using IVOESpaces.Shell.Spaces;

namespace IVOESpaces.Shell.Settings;

/// <summary>
/// Transitional settings host entrypoint. It establishes the future page-based
/// architecture while delegating to the existing SettingsWindow implementation
/// until all tabs are migrated.
/// </summary>
internal static class SettingsWindowHost
{
    public static void ShowOrFocus(SpaceManager spaceManager, IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        SettingsWindow.ShowOrFocus(spaceManager, pluginSettings);
    }
}
