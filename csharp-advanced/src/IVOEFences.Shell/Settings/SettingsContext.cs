using IVOEFences.Core.Models;
using IVOEFences.Core.Plugins;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Fences;

namespace IVOEFences.Shell.Settings;

internal sealed class SettingsContext
{
    public FenceManager FenceManager { get; }
    public IReadOnlyList<PluginSettingDefinition> PluginSettings { get; }

    public SettingsContext(
        FenceManager fenceManager,
        IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        FenceManager = fenceManager;
        PluginSettings = pluginSettings;
    }

    public AppSettings Settings => AppSettingsRepository.Instance.Current;

    public FenceWindow? GetSelectedFence(Guid? selectedFenceId)
    {
        List<FenceWindow> windows = FenceManager.Windows.ToList();
        if (windows.Count == 0)
            return null;

        if (selectedFenceId.HasValue)
        {
            FenceWindow? selected = windows.FirstOrDefault(w => w.ModelId == selectedFenceId.Value);
            if (selected != null)
                return selected;
        }

        return windows[0];
    }
}
