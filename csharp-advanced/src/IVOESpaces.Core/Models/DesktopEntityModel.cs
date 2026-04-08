namespace IVOESpaces.Core.Models;

/// <summary>
/// Durable desktop entity tracked independently from space view records.
/// This identity survives space membership changes and path renames.
/// </summary>
public sealed record DesktopEntityModel
{
    public Guid Id { get; init; } = Guid.NewGuid();
    public string DisplayName { get; set; } = string.Empty;
    public string ParsingName { get; set; } = string.Empty;
    public string? FileSystemPath { get; set; }
    public bool IsDirectory { get; set; }
    public bool IsShortcut { get; set; }
    public string? Extension { get; set; }
    public DesktopItemOwnership Ownership { get; set; } = DesktopItemOwnership.DesktopOnly;
    public Guid? OwnerSpaceId { get; set; }
    public DateTime LastSeenUtc { get; set; } = DateTime.UtcNow;
}
