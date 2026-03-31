namespace IVOEFences.Core.Models;

public enum FenceType { Standard, Portal }
public enum DockEdge { None, Top, Bottom, Left, Right }

public record FenceModel
{
    public Guid   Id    { get; init; }  = Guid.NewGuid();
    public string Title { get; set; }   = "New Fence";

    // Position stored as fractions of the owning monitor's work area (0.0–1.0).
    // NEVER store raw pixels — they drift when DPI, resolution, or taskbar size changes.
    public double XFraction      { get; set; }
    public double YFraction      { get; set; }
    public double WidthFraction  { get; set; } = 0.15;
    public double HeightFraction { get; set; } = 0.20;

    // Monitor identity — by DeviceName e.g. "\\.\DISPLAY2"
    public string MonitorDeviceName { get; set; } = string.Empty;

    // Per-fence visual overrides (null = use global theme)
    public string? BackgroundColorOverride { get; set; }
    public string? TitleColorOverride      { get; set; }
    public string? IconTintColorOverride   { get; set; } // Step 36: Per-fence icon tint (ARGB hex)
    public double? CornerRadiusOverride    { get; set; }
    public double? OpacityOverride         { get; set; }

    // Behavior
    public bool   IsRolledUp              { get; set; }
    public double PreRollupHeightFraction { get; set; }
    public bool   IsLocked                { get; set; }
    public bool   AutoHide                { get; set; }
    public bool   IsHidden                { get; set; } // Step 27: Quick-hide state
    public bool   IsAiSuggested           { get; set; }
    public DateTime? AiSuggestedAtUtc     { get; set; }
    public string SortMode                { get; set; } = "Manual";
    public int?   IconSizeOverride        { get; set; }
    public FenceSettingsOverrides SettingsOverrides { get; set; } = new();
    public bool   IsDynamicVisibilityEnabled { get; set; }
    public int    VisibleFromHour         { get; set; } = 0;
    public int    VisibleToHour           { get; set; } = 23;
    public List<string> ContextTags       { get; set; } = new();
    public string? AssignedProfileId      { get; set; }

    // Type
    public FenceType Type             { get; set; } = FenceType.Standard;
    public string?   PortalFolderPath { get; set; }

    // Quick-launch bar mode
    public bool     IsBar              { get; set; }
    public DockEdge DockEdge           { get; set; } = DockEdge.None;
    public bool     ReserveScreenSpace { get; set; }
    public int      BarThickness       { get; set; } = 56;

    // Desktop page membership (0-indexed)
    public int PageIndex { get; set; } = 0;

    // Tabs & Container (Step 35)
    // If TabContainerId is null, this fence is standalone.
    // If TabContainerId is set, this fence is part of a tabbed container.
    public Guid? TabContainerId { get; set; } = null;
    
    // Position within a tab container (0-based index).
    // Used only when TabContainerId is not null.
    public int TabIndex { get; set; } = 0;

    // Items — not used for Portal fences (they read from filesystem at runtime)
    public List<FenceItemModel> Items { get; set; } = new();
}
