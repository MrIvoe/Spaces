namespace IVOEFences.Core.Plugins;

public sealed record PluginInstallResult(
    bool Success,
    string Message,
    string? PluginId = null,
    string? Version = null,
    string? InstallDirectory = null,
    string? BackupDirectory = null);
