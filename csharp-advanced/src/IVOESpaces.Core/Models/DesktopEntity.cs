namespace IVOESpaces.Core.Models;

public enum DesktopEntityKind
{
    File,
    Directory,
    Shortcut,
    VirtualMarker,
}

public enum DesktopOwnershipMode
{
    DesktopOnly,
    SpaceManaged,
    PortalManaged,
    WorkspaceProjected,
}

/// <summary>
/// Compatibility projection over the existing durable desktop entity state.
/// </summary>
public sealed record DesktopEntity
{
    public Guid Id { get; init; } = Guid.NewGuid();
    public string DisplayName { get; set; } = string.Empty;
    public string ParsingPath { get; set; } = string.Empty;
    public string? FileSystemPath { get; set; }
    public DesktopEntityKind Kind { get; set; }
    public string? Extension { get; set; }
    public DesktopOwnershipMode OwnershipMode { get; set; } = DesktopOwnershipMode.DesktopOnly;
    public Guid? OwnerSpaceId { get; set; }
    public DateTime LastSeenUtc { get; set; } = DateTime.UtcNow;
    public bool IsMissing { get; set; }
    public bool IsPinned { get; set; }
    public List<string> Tags { get; set; } = new();
}
