using IVOESpaces.Core.Models;
using IVOESpaces.Core.Plugins;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Spaces;

namespace IVOESpaces.Shell.Settings;

internal sealed class SettingsContext
{
    public SpaceManager SpaceManager { get; }
    public IReadOnlyList<PluginSettingDefinition> PluginSettings { get; }
    public PluginManagerService PluginManager { get; }
    public PluginUpdateService PluginUpdate { get; }
    public ThemeService ThemeService { get; }

    public SettingsContext(
        SpaceManager spaceManager,
        IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        SpaceManager = spaceManager;
        PluginSettings = pluginSettings;
        PluginManager = PluginManagerService.Instance;
        PluginUpdate = PluginUpdateService.Instance;
        ThemeService = ThemeService.Instance;
    }

    public AppSettings Settings => AppSettingsRepository.Instance.Current;

    public SpaceWindow? GetSelectedSpace(Guid? selectedSpaceId)
    {
        List<SpaceWindow> windows = SpaceManager.Windows.ToList();
        if (windows.Count == 0)
            return null;

        if (selectedSpaceId.HasValue)
        {
            SpaceWindow? selected = windows.FirstOrDefault(w => w.ModelId == selectedSpaceId.Value);
            if (selected != null)
                return selected;
        }

        return windows[0];
    }
}
