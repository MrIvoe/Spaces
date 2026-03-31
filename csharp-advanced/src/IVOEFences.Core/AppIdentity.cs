namespace IVOEFences.Core;

/// <summary>
/// Single source of truth for all branding and identity constants.
/// Every user-facing string, window class, and registry key references this.
/// </summary>
public static class AppIdentity
{
    // Branding
    public const string ProductName = "IVOE Fences";
    public const string InternalName = "IVOEFences";

    // Legacy migration support
    public const string LegacyName = "OpenFences";

    // UI
    public const string SettingsWindowTitle = ProductName + " Settings";
    public const string SearchDialogTitle = ProductName + " Search";
    public const string CommandPaletteTitle = ProductName + " Command Palette";
    public const string TrayTooltip = ProductName;

    // System integration
    public const string StartupRegistryName = InternalName;

    // Window classes
    public const string TrayWindowClass = InternalName + "_TrayHost";
    public const string SettingsWindowClass = InternalName + "_SettingsWindow";
}
