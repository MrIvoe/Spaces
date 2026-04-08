using System.Text.Json;
using IVOESpaces.Core;

namespace IVOESpaces.Core.Services;

public sealed class PluginHostSettingsStore
{
    private sealed class HostSettingsDto
    {
        public Dictionary<string, bool> EnabledStates { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    }

    private static readonly Lazy<PluginHostSettingsStore> _instance = new(() => new PluginHostSettingsStore());
    private readonly object _lock = new();
    private readonly Dictionary<string, bool> _enabledStates = new(StringComparer.OrdinalIgnoreCase);

    public static PluginHostSettingsStore Instance => _instance.Value;

    private PluginHostSettingsStore()
    {
        Load();
    }

    public bool GetEnabled(string pluginId, bool fallback = true)
    {
        lock (_lock)
        {
            return _enabledStates.TryGetValue(pluginId, out bool enabled) ? enabled : fallback;
        }
    }

    public void SetEnabled(string pluginId, bool enabled)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return;

        lock (_lock)
        {
            _enabledStates[pluginId] = enabled;
            Save_NoLock();
        }
    }

    private void Load()
    {
        lock (_lock)
        {
            try
            {
                if (!File.Exists(AppPaths.PluginHostConfig))
                    return;

                string json = File.ReadAllText(AppPaths.PluginHostConfig);
                HostSettingsDto? dto = JsonSerializer.Deserialize<HostSettingsDto>(json);
                _enabledStates.Clear();
                if (dto?.EnabledStates != null)
                {
                    foreach ((string key, bool value) in dto.EnabledStates)
                        _enabledStates[key] = value;
                }
            }
            catch
            {
                _enabledStates.Clear();
            }
        }
    }

    private void Save_NoLock()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.PluginHostConfig) ?? AppPaths.DataRoot);
        HostSettingsDto dto = new() { EnabledStates = new Dictionary<string, bool>(_enabledStates, StringComparer.OrdinalIgnoreCase) };
        string json = JsonSerializer.Serialize(dto, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(AppPaths.PluginHostConfig, json);
    }
}
