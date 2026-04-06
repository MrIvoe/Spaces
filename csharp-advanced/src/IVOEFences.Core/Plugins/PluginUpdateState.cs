namespace IVOEFences.Core.Plugins;

public enum PluginUpdateStateKind
{
    Unknown,
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Failed,
}

public sealed record PluginUpdateState(
    PluginUpdateStateKind Kind,
    string Message,
    string? LatestVersion = null,
    DateTimeOffset? TimestampUtc = null)
{
    public static PluginUpdateState Create(
        PluginUpdateStateKind kind,
        string message,
        string? latestVersion = null)
    {
        return new PluginUpdateState(kind, message, latestVersion, DateTimeOffset.UtcNow);
    }
}
