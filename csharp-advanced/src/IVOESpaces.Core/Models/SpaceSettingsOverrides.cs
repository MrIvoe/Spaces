using System.Collections.Generic;

namespace IVOESpaces.Core.Models;

public sealed class SpaceSettingsOverrides
{
    // Appearance
    public string? TitlebarModeOverride { get; set; }
    public bool? BlurEnabledOverride { get; set; }
    public string? CornerStyleOverride { get; set; }
    public string? IconLayoutOverride { get; set; }
    public string? TitleFontOverride { get; set; }
    public string? TitleFontColorOverride { get; set; }

    // Behavior
    public string? RollupModeOverride { get; set; }
    public string? QuickHideModeOverride { get; set; }
    public string? SnapModeOverride { get; set; }
    public bool? PortalEnabledOverride { get; set; }
    public string? LiveFolderViewOverride { get; set; }
    public string? SortModeOverride { get; set; }

    // Rules
    public List<string> IncludeRules { get; set; } = new();
    public List<string> ExcludeRules { get; set; } = new();

    // Advanced
    public bool? LockSpaceOverride { get; set; }
    public bool? PerMonitorOverride { get; set; }
    public bool? DesktopDoubleClickJumpOverride { get; set; }
}
