using IVOESpaces.Core.Plugins;

namespace IVOESpaces.Core.Services;

public sealed class PluginUpdateService
{
    private static readonly Lazy<PluginUpdateService> _instance = new(() => new PluginUpdateService());
    private readonly object _lock = new();
    private PluginUpdateState _globalState = PluginUpdateState.Create(PluginUpdateStateKind.Idle, "Plugin updates idle.");
    private readonly Dictionary<string, PluginUpdateState> _pluginStates = new(StringComparer.OrdinalIgnoreCase);

    public static PluginUpdateService Instance => _instance.Value;

    private PluginUpdateService()
    {
    }

    public PluginUpdateState GetGlobalState()
    {
        lock (_lock)
        {
            return _globalState;
        }
    }

    public PluginUpdateState GetPluginState(string pluginId)
    {
        lock (_lock)
        {
            if (string.IsNullOrWhiteSpace(pluginId))
                return _globalState;

            return _pluginStates.TryGetValue(pluginId, out PluginUpdateState? state)
                ? state
                : PluginUpdateState.Create(PluginUpdateStateKind.Unknown, "No update state is known for this plugin.");
        }
    }

    public void BeginCheck(string reason)
    {
        lock (_lock)
        {
            _globalState = PluginUpdateState.Create(PluginUpdateStateKind.Checking, $"Checking plugin updates ({reason}).");
        }
    }

    public void SetGlobalState(PluginUpdateState state)
    {
        lock (_lock)
        {
            _globalState = state;
        }
    }

    public void SetPluginState(string pluginId, PluginUpdateState state)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return;

        lock (_lock)
        {
            _pluginStates[pluginId] = state;
        }
    }

    public void RefreshFromMetadata(IReadOnlyList<PluginMetadata> metadata)
    {
        lock (_lock)
        {
            foreach (PluginMetadata plugin in metadata)
            {
                if (!_pluginStates.ContainsKey(plugin.Id))
                {
                    _pluginStates[plugin.Id] = PluginUpdateState.Create(
                        PluginUpdateStateKind.UpToDate,
                        "No update feed declared; using installed version.",
                        plugin.Version);
                }
            }

            _globalState = PluginUpdateState.Create(PluginUpdateStateKind.Idle, "Plugin update state refreshed.");
        }
    }

    public void CheckFromManifests(IReadOnlyList<PluginManifestReader.PluginManifest> manifests)
    {
        lock (_lock)
        {
            int updateCount = 0;
            foreach (PluginManifestReader.PluginManifest manifest in manifests)
            {
                bool hasUpdate = IsVersionNewer(manifest.Version, manifest.LatestVersion);
                if (hasUpdate)
                {
                    ++updateCount;
                    _pluginStates[manifest.Id] = PluginUpdateState.Create(
                        PluginUpdateStateKind.UpdateAvailable,
                        $"Update available for {manifest.Name}",
                        manifest.LatestVersion);
                }
                else
                {
                    _pluginStates[manifest.Id] = PluginUpdateState.Create(
                        PluginUpdateStateKind.UpToDate,
                        "Up to date",
                        manifest.Version);
                }
            }

            _globalState = updateCount > 0
                ? PluginUpdateState.Create(PluginUpdateStateKind.UpdateAvailable, $"{updateCount} plugin update(s) available.")
                : PluginUpdateState.Create(PluginUpdateStateKind.UpToDate, "All plugins are up to date.");
        }
    }

    private static bool IsVersionNewer(string currentVersion, string? latestVersion)
    {
        if (string.IsNullOrWhiteSpace(latestVersion))
            return false;

        if (!Version.TryParse(NormalizeVersion(currentVersion), out Version? current))
            return false;
        if (!Version.TryParse(NormalizeVersion(latestVersion), out Version? latest))
            return false;

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
