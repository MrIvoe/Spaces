namespace IVOEFences.Core.Models;

public sealed record WorkspaceProfileBinding
{
    public string ProfileId { get; init; } = "default";
    public string? SnapshotId { get; set; }
    public Dictionary<string, string> GlobalSettingsOverrides { get; init; } = new();
    public Dictionary<Guid, Dictionary<string, string>> FenceOverrides { get; init; } = new();
}