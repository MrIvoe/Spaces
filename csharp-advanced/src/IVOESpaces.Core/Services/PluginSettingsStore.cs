using System.Text.Json;

namespace IVOESpaces.Core.Services;

public sealed class PluginSettingsStore
{
    private static readonly Lazy<PluginSettingsStore> _instance = new(() => new PluginSettingsStore());

    private readonly object _lock = new();
    private Dictionary<string, string> _values = new();

    public static PluginSettingsStore Instance => _instance.Value;

    private string FilePath => AppPaths.PluginSettingsConfig;

    private PluginSettingsStore()
    {
        Load();
    }

    public string? Get(string pluginId, string key, string? fallback = null)
    {
        lock (_lock)
        {
            return _values.TryGetValue(BuildCompositeKey(pluginId, key), out string? value)
                ? value
                : fallback;
        }
    }

    public void Set(string pluginId, string key, string value)
    {
        lock (_lock)
        {
            _values[BuildCompositeKey(pluginId, key)] = value;
            Save_NoLock();
        }

        SettingsEvents.Raise($"plugin.{pluginId}.{key}", "plugin", value);
    }

    private void Load()
    {
        lock (_lock)
        {
            try
            {
                if (!File.Exists(FilePath))
                    return;

                string json = File.ReadAllText(FilePath);
                _values = JsonSerializer.Deserialize<Dictionary<string, string>>(json) ?? new Dictionary<string, string>();
            }
            catch
            {
                _values = new Dictionary<string, string>();
            }
        }
    }

    private void Save_NoLock()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        string json = JsonSerializer.Serialize(_values, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(FilePath, json);
    }

    private static string BuildCompositeKey(string pluginId, string key) => $"{pluginId}:{key}";
}