using IVOESpaces.Core.Plugins;
using IVOESpaces.Core;

namespace IVOESpaces.Core.Services;

public sealed class PluginManagerService
{
    private sealed class PluginRecord
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public string Version { get; set; } = "0.0.0";
        public string Description { get; set; } = string.Empty;
        public string Author { get; set; } = "unknown";
        public List<string> Capabilities { get; } = new();
        public string SourcePath { get; set; } = string.Empty;
        public bool IsLoaded { get; set; }
        public bool IsEnabled { get; set; } = true;
        public string Compatibility { get; set; } = "Unknown";
        public bool UpdateAvailable { get; set; }
        public string? LatestVersion { get; set; }
        public string Status { get; set; } = "Discovered";
    }

    private static readonly Lazy<PluginManagerService> _instance = new(() => new PluginManagerService());
    private readonly object _lock = new();
    private readonly Dictionary<string, PluginRecord> _plugins = new(StringComparer.OrdinalIgnoreCase);
    private readonly PluginCompatibilityService _compatibilityService;
    private readonly PluginHostSettingsStore _hostSettingsStore;

    public static PluginManagerService Instance => _instance.Value;

    private PluginManagerService()
    {
        _compatibilityService = PluginCompatibilityService.Instance;
        _hostSettingsStore = PluginHostSettingsStore.Instance;
    }

    public void RefreshFromManifests(string pluginsRoot, PluginManifestReader manifestReader)
    {
        List<PluginManifestReader.PluginManifest> manifests = new();
        manifests.AddRange(manifestReader.ReadInstalled(AppPaths.PluginInstalledDir));
        manifests.AddRange(manifestReader.ReadAll(pluginsRoot));
        RefreshFromManifestList(manifests);
    }

    public void RefreshFromManifestList(IReadOnlyList<PluginManifestReader.PluginManifest> manifests)
    {
        var latestById = manifests
            .GroupBy(m => m.Id, StringComparer.OrdinalIgnoreCase)
            .Select(g => g.OrderByDescending(m => ParseVersion(m.Version)).First())
            .ToList();

        lock (_lock)
        {
            foreach (PluginManifestReader.PluginManifest manifest in latestById)
            {
                if (!_plugins.TryGetValue(manifest.Id, out PluginRecord? existing))
                {
                    existing = new PluginRecord { Id = manifest.Id };
                    _plugins[manifest.Id] = existing;
                }

                PluginCompatibilityResult compatibility = _compatibilityService.Evaluate(manifest);

                existing.Name = manifest.Name;
                existing.Version = manifest.Version;
                existing.Description = manifest.Description;
                existing.Author = manifest.Author;
                existing.SourcePath = manifest.DirectoryPath;
                existing.Status = existing.IsLoaded ? existing.Status : "Discovered";
                existing.IsEnabled = _hostSettingsStore.GetEnabled(manifest.Id, true);
                existing.Compatibility = compatibility.Reason;
                existing.LatestVersion = manifest.LatestVersion;
                existing.UpdateAvailable = IsVersionNewer(existing.Version, existing.LatestVersion);

                existing.Capabilities.Clear();
                existing.Capabilities.AddRange(manifest.Capabilities);
            }
        }
    }

    public void RegisterLoadedPlugin(ISpacePlugin plugin, string sourcePath)
    {
        lock (_lock)
        {
            if (!_plugins.TryGetValue(plugin.Id, out PluginRecord? record))
            {
                record = new PluginRecord
                {
                    Id = plugin.Id,
                    Name = plugin.Name,
                    Version = string.IsNullOrWhiteSpace(plugin.Version) ? "0.0.0" : plugin.Version,
                    SourcePath = sourcePath,
                };
                _plugins[plugin.Id] = record;
            }

            record.Name = string.IsNullOrWhiteSpace(plugin.Name) ? plugin.Id : plugin.Name;
            record.Version = string.IsNullOrWhiteSpace(plugin.Version) ? record.Version : plugin.Version;
            record.SourcePath = sourcePath;
            record.IsLoaded = true;
            record.IsEnabled = _hostSettingsStore.GetEnabled(plugin.Id, true);
            record.Status = "Loaded";
        }
    }

    public bool IsPluginEnabled(string pluginId)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return false;

        lock (_lock)
        {
            if (_plugins.TryGetValue(pluginId, out PluginRecord? record))
                return record.IsEnabled;
        }

        return _hostSettingsStore.GetEnabled(pluginId, true);
    }

    public void SetPluginEnabled(string pluginId, bool enabled)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return;

        _hostSettingsStore.SetEnabled(pluginId, enabled);
        lock (_lock)
        {
            if (_plugins.TryGetValue(pluginId, out PluginRecord? record))
            {
                record.IsEnabled = enabled;
                if (!enabled)
                    record.Status = "Disabled";
            }
        }
    }

    public void SetPluginStatus(string pluginId, string status)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return;

        lock (_lock)
        {
            if (!_plugins.TryGetValue(pluginId, out PluginRecord? record))
            {
                record = new PluginRecord { Id = pluginId, Name = pluginId };
                _plugins[pluginId] = record;
            }

            record.Status = string.IsNullOrWhiteSpace(status) ? record.Status : status;
        }
    }

    public IReadOnlyList<PluginMetadata> GetPlugins()
    {
        lock (_lock)
        {
            return _plugins.Values
                .OrderBy(p => p.Name, StringComparer.OrdinalIgnoreCase)
                .Select(p => new PluginMetadata(
                    p.Id,
                    p.Name,
                    p.Version,
                    p.Description,
                    p.Author,
                    p.Capabilities.ToArray(),
                    p.SourcePath,
                    p.IsLoaded,
                    p.Status,
                    p.IsEnabled,
                    p.Compatibility,
                    p.UpdateAvailable,
                    p.LatestVersion))
                .ToList();
        }
    }

    private static Version ParseVersion(string value)
    {
        return Version.TryParse(NormalizeVersion(value), out Version? parsed)
            ? parsed
            : new Version(0, 0, 0);
    }

    private static bool IsVersionNewer(string currentVersion, string? latestVersion)
    {
        if (string.IsNullOrWhiteSpace(latestVersion))
            return false;

        Version current = ParseVersion(currentVersion);
        Version latest = ParseVersion(latestVersion);
        return latest > current;
    }

    private static string NormalizeVersion(string raw)
    {
        string[] parts = raw.Split('.', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 1) return raw + ".0.0";
        if (parts.Length == 2) return raw + ".0";
        return raw;
    }
}
