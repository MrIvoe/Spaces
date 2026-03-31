namespace IVOEFences.Core.Models;

public class AppSettings
{
    // General
    public bool   ShowFencesOnStartup          { get; set; } = true;
    public bool   HideDesktopIconsOutsideFences { get; set; } = false;
    public bool   AutoArrangeOnStartup         { get; set; } = true;
    public bool   UseAiDefaultMode             { get; set; } = false;
    public string GlobalRollupMode             { get; set; } = "ClickToOpen";
    public string GlobalTitlebarMode           { get; set; } = "Visible";
    public bool   KeepFencesOnTopPeekMode      { get; set; } = false;
    public string ToggleHotkey                 { get; set; } = "Win+Space";
    public string SearchHotkey                 { get; set; } = "Win+F";
    public bool   StartWithWindows             { get; set; } = true;
    public bool   EnableGlobalHotkeys          { get; set; } = true;
    public int    PeekDelayMs                  { get; set; } = 500;
    public string DefaultSortMode              { get; set; } = "Name";
    public bool   AutoResizeFencesOnIconSizeChange { get; set; } = true;
    public bool   HighDpiPerMonitorScaling     { get; set; } = true;
    public bool   AutoHideInFullscreenApps     { get; set; } = false; // Gaming mode: hide fences when a fullscreen app is active
    public int    IdleThresholdSeconds         { get; set; } = 300;  // Mouse idle → fade fences after this many seconds (0 = disabled)
    public int    IdleFadeOpacity              { get; set; } = 30;   // Opacity % to apply to fences when idle (0–100)

    // Versioning (increment CurrentSettingsVersion when adding breaking changes)
    public int    SettingsVersion              { get; set; } = 0;

    // Appearance
    public string ThemeMode             { get; set; } = "Auto"; // Auto|Light|Dark|Accent
    public bool   UseSystemAccentColor  { get; set; } = true;
    public int    FenceOpacity          { get; set; } = 85;     // 20–100
    public int    CornerRadius          { get; set; } = 8;      // 0–20
    public bool   ShowFenceTitles       { get; set; } = true;
    public int    TitleBarHeight        { get; set; } = 24;     // 18–32 px at 96 DPI
    public int    IconSize              { get; set; } = 48;     // 32|48|64|96
    public int    IconSpacing           { get; set; } = 8;      // 2–20 px
    public bool   BlurBackground        { get; set; } = false;
    public int    GlassStrength         { get; set; } = 50;     // 0-100

    // Search & filter
    public bool   EnableFenceSearch     { get; set; } = true;
    public bool   SearchIsCaseSensitive { get; set; } = false;
    public bool   SearchOnTypeInWindow  { get; set; } = false;  // if true, typing in fence window auto-opens search
    public bool   EnableCommandPalette  { get; set; } = true;
    public bool   EnableQuickActions    { get; set; } = true;
    public bool   EnableHoverPreviews   { get; set; } = true;
    public bool   EnableScriptActions   { get; set; } = false;
    public bool   EnableAnimations      { get; set; } = true;
    public bool   EnableGlobalPlacementRules { get; set; } = true;
    public bool   EnableQuickHideMode   { get; set; } = true;
    public bool   RollupRequiresClick   { get; set; } = false;
    public bool   RollupRequiresHover   { get; set; } = false;
    public bool   EnableDesktopPages    { get; set; } = false;
    public int    CurrentDesktopPage    { get; set; } = 0;
    public int    DesktopPageCount      { get; set; } = 1;
    public string UiLanguage            { get; set; } = "en-US";

    // Profiles & dynamic fences
    public bool   EnableDynamicFences   { get; set; } = true;
    public int    AiSuggestionClusterCount { get; set; } = 3;
    public bool   EnableAiDynamicResizing { get; set; } = true;
    public bool   AutoSwitchProfiles    { get; set; } = true;
    public string ActiveProfileId       { get; set; } = "default";

    // Snap & grid
    public bool   SnapToScreenEdges     { get; set; } = true;
    public bool   SnapToOtherFences     { get; set; } = true;
    public bool   SnapToMonitorCenter   { get; set; } = true;
    public int    SnapThreshold         { get; set; } = 12;
    public bool   SnapToGrid            { get; set; } = false;
    public int    InterFenceSpacing     { get; set; } = 12;
    public bool   ShowGridWhileDragging { get; set; } = false;
    public int    GridSize              { get; set; } = 24;
    public bool   ShowDistributionGuides{ get; set; } = true;
    public bool   RollUpOnEdgeSnap      { get; set; } = false;
    public bool   DetectMonitorConfigurationChanges { get; set; } = true;
    public bool   AutoSwapMisplacedFenceContents { get; set; } = true;

    // Folder portal integration
    public bool   EnableFolderPortalNavigation { get; set; } = true;
    public bool   LiveFolderShowIcon { get; set; } = true;
    public string LiveFolderDefaultView { get; set; } = "Icons";
    public bool   OpenPortalFoldersInSeparateExplorerProcess { get; set; } = false;
    public string FolderPortalScrollbarBehavior { get; set; } = "Auto";
    public bool   EnableFastFolderPortalResizing { get; set; } = true;

    // Drag & drop policy
    public string StandardFenceDropMode { get; set; } = "Reference";
    public bool   ConfirmExternalDrops { get; set; } = false;
    public bool   AutoApplyRulesOnDrop { get; set; } = true;
    public bool   HighlightDropTargets { get; set; } = true;

    // User-defined profiles (persisted; built-ins are reconstructed on startup)
    public List<FenceProfileModel> UserProfiles { get; set; } = new();

    // Advanced
    public int    WatchdogIntervalSeconds { get; set; } = 30;
    public int    IconCacheMaxEntries     { get; set; } = 200;
    public string LogLevel               { get; set; } = "Info";
    public bool   EnableAdvancedLogging  { get; set; } = false;
    public string TrayLeftClickAction    { get; set; } = "OpenSettings"; // OpenSettings | ToggleFences | ShowMenu
    public bool   ForceUnloadDllAfterStartup { get; set; } = false;
    public bool   EnableUacPromptMitigation { get; set; } = true;
}
