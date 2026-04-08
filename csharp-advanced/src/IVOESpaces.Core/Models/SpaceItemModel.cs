namespace IVOESpaces.Core.Models;

public record SpaceItemModel
{
    public Guid    Id               { get; init; } = Guid.NewGuid();
    // Durable entity reference. Space items are view placement records, not file owners.
    public Guid    DesktopEntityId  { get; set; }
    // Transitional alias to align with the entity-centric model naming.
    public Guid    EntityId
    {
        get => DesktopEntityId;
        set => DesktopEntityId = value;
    }
    public string  DisplayName      { get; set; }  = string.Empty;
    // Runtime path cache for renderer/open operations.
    // This no longer implies filesystem ownership.
    public string  TargetPath       { get; set; }  = string.Empty;
    public string? IconOverridePath { get; set; }
    public int     GridColumn       { get; set; }
    public int     GridRow          { get; set; }
    // True when TargetPath no longer exists — item shown grayed out, never silently deleted
    public bool    IsUnresolved     { get; set; }
    // True if TargetPath is a directory, false if file
    public bool    IsDirectory      { get; set; }
    // Sort order in space (0-based index)
    public int     SortOrder        { get; set; }
    public bool    IsFromDesktop    { get; set; }

    // Usage tracking for AI-based icon grouping suggestions
    // Last time this item was opened/executed (UTC)
    public DateTime? LastOpenedTime  { get; set; }
    // Total number of times this item has been opened
    public int       OpenCount       { get; set; } = 0;
    // Cached file extension for grouping (e.g., ".exe", ".docx", ".jpg")
    public string?   TrackedFileType { get; set; }
    // Cached file size in bytes for grouping (updated periodically)
    public long      TrackedFileSize { get; set; } = 0;

    // Metadata & quick-action state
    public List<string> Tags { get; set; } = new();
    public string? CustomLabel { get; set; }
    public bool IsPinned { get; set; }
    public DateTime? LastPreviewedTime { get; set; }
    public DateTime? LastSuggestedMoveTime { get; set; }
}
