namespace IVOEFences.Core.Plugins;

public sealed record PluginMetadata(
    string Id,
    string Name,
    string Version,
    string Description,
    string Author,
    IReadOnlyList<string> Capabilities,
    string SourcePath,
    bool IsLoaded,
    string Status,
    bool IsEnabled,
    string Compatibility,
    bool UpdateAvailable,
    string? LatestVersion);
