using System;
using System.Collections.Generic;
using System.Linq;

namespace IVOESpaces.Shell.Settings;

internal enum SettingsScope
{
    Global,
    Space,
}

internal enum SettingValueKind
{
    Toggle,
    Choice,
    Number,
    Color,
    Text,
    Action,
}

internal sealed record SettingDefinition(
    string Key,
    SettingsScope Scope,
    string Tab,
    string Section,
    string Label,
    string Tooltip,
    SettingValueKind Kind,
    string[]? Choices = null,
    bool IsBeta = false);

internal static class SettingsBlueprint
{
    public static IReadOnlyList<SettingDefinition> GlobalSettings { get; } = new List<SettingDefinition>
    {
        new("global.rollup.mode", SettingsScope.Global, "Display", "Behavior", "Global roll-up mode", "Controls how all spaces expand: click, hover, or always expanded.", SettingValueKind.Choice,
            new[] { "ClickToOpen", "HoverToOpen", "AlwaysExpanded" }),
        new("global.titlebar.mode", SettingsScope.Global, "Display", "Behavior", "Global titlebar mode", "Show titlebars always, only on mouseover, or hide them.", SettingValueKind.Choice,
            new[] { "Visible", "Mouseover", "Hidden" }),
        new("global.peek.keepOnTop", SettingsScope.Global, "Display", "Behavior", "Peek mode keep-on-top", "Temporarily keep spaces visible above desktop clutter during peek interactions.", SettingValueKind.Toggle),
        new("global.blur.enabled", SettingsScope.Global, "Display", "Appearance", "Blur behind spaces", "Enable acrylic/blur treatment behind spaces where supported.", SettingValueKind.Toggle),
        new("global.hidpi.perMonitor", SettingsScope.Global, "Display", "Appearance", "Per-monitor DPI scaling", "Scale titlebars and space internals for each monitor DPI.", SettingValueKind.Toggle),

        new("global.snap.edges", SettingsScope.Global, "Desktop", "Snap", "Snap to screen edges", "Snap spaces to monitor bounds while dragging.", SettingValueKind.Toggle),
        new("global.snap.grid", SettingsScope.Global, "Desktop", "Snap", "Snap to grid", "Align space movement and resize to grid intervals.", SettingValueKind.Toggle),
        new("global.spacing", SettingsScope.Global, "Desktop", "Snap", "Inter-space spacing", "Minimum spacing between spaces when snapping.", SettingValueKind.Number),
        new("global.dragdrop.standardMode", SettingsScope.Global, "Desktop", "Drag & Drop", "Standard space drop behavior", "Choose whether dropped items are referenced, moved into space storage, or added as shortcuts.", SettingValueKind.Choice,
            new[] { "Reference", "MoveIntoSpaceStorage", "Shortcut" }),
        new("global.dragdrop.confirmExternal", SettingsScope.Global, "Desktop", "Drag & Drop", "Confirm external drops", "Prompt before importing files dragged from outside the desktop.", SettingValueKind.Toggle),
        new("global.dragdrop.applyRules", SettingsScope.Global, "Desktop", "Drag & Drop", "Apply rules after drop", "Run placement rules immediately after accepting a drop.", SettingValueKind.Toggle),
        new("global.dragdrop.highlightTargets", SettingsScope.Global, "Desktop", "Drag & Drop", "Highlight drop targets", "Show space highlight while a drag operation is hovering.", SettingValueKind.Toggle),
        new("global.monitor.detectChanges", SettingsScope.Global, "Desktop", "Multi-Monitor", "Detect monitor layout changes", "Track monitor topology changes and activate matching layout profile.", SettingValueKind.Toggle),
        new("global.monitor.autoSwap", SettingsScope.Global, "Desktop", "Multi-Monitor", "Auto-swap misplaced groups", "Try to recover misplaced space groups after monitor sleep/reconnect.", SettingValueKind.Toggle),

        new("global.portal.enabled", SettingsScope.Global, "Folder Portals", "Integration", "Enable folder portals", "Allow spaces to render live folder contents.", SettingValueKind.Toggle),
        new("global.portal.separateExplorer", SettingsScope.Global, "Folder Portals", "Integration", "Open folders in separate Explorer process", "Launch portal targets in dedicated explorer.exe process.", SettingValueKind.Toggle),
        new("global.portal.scrollbar", SettingsScope.Global, "Folder Portals", "Performance", "Portal scrollbar behavior", "Control portal scrollbar mode for compact and large content views.", SettingValueKind.Choice,
            new[] { "Auto", "AlwaysVisible", "Hidden" }),
        new("global.portal.fastResize", SettingsScope.Global, "Folder Portals", "Performance", "Fast portal resize", "Prioritize portal redraw speed during resize operations.", SettingValueKind.Toggle),

        new("global.rules.enabled", SettingsScope.Global, "Rules", "Organization", "Enable global placement rules", "Apply rule-based auto-placement to new desktop items.", SettingValueKind.Toggle),
        new("global.quickHide.enabled", SettingsScope.Global, "Rules", "Organization", "Quick-hide unmanaged icons", "Hide desktop items that are not assigned to a space.", SettingValueKind.Toggle),
        new("global.hotkeys.enabled", SettingsScope.Global, "Hotkeys", "Keyboard", "Enable global hotkeys", "Master switch for keyboard shortcuts.", SettingValueKind.Toggle),
        new("global.language", SettingsScope.Global, "General", "Localization", "Language", "UI language/localization pack identifier.", SettingValueKind.Text),

        new("global.logging.verbose", SettingsScope.Global, "Advanced", "Diagnostics", "Advanced logging", "Enable verbose diagnostics and recovery traces.", SettingValueKind.Toggle),
        new("global.snapshot.backup", SettingsScope.Global, "Advanced", "Recovery", "Backup layout", "Create a full space snapshot.", SettingValueKind.Action),
        new("global.snapshot.restore", SettingsScope.Global, "Advanced", "Recovery", "Restore layout", "Restore a previously saved snapshot.", SettingValueKind.Action),
        new("global.forceUnloadDll", SettingsScope.Global, "Advanced", "Compatibility", "Force-unload shell DLL after startup", "Aggressive compatibility toggle for shell extension startup behavior.", SettingValueKind.Toggle, IsBeta: true),
        new("global.uac.resolve", SettingsScope.Global, "Advanced", "Compatibility", "UAC prompt mitigation", "Use safer process launch behavior to minimize repeated UAC prompts.", SettingValueKind.Toggle),
    };

    public static IReadOnlyList<SettingDefinition> SpaceSettings { get; } = new List<SettingDefinition>
    {
        new("space.titlebar.mode", SettingsScope.Space, "Appearance", "Titlebar", "Titlebar mode", "Visible, hidden, or mouseover-only titlebar behavior for this space.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Visible", "Mouseover", "Hidden" }),
        new("space.color.background", SettingsScope.Space, "Appearance", "Visual", "Background color", "Optional per-space background color override.", SettingValueKind.Color),
        new("space.opacity", SettingsScope.Space, "Appearance", "Visual", "Transparency", "Optional per-space opacity override.", SettingValueKind.Number),
        new("space.blur.enabled", SettingsScope.Space, "Appearance", "Visual", "Blur", "Enable/disable blur for this space.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Enabled", "Disabled" }),
        new("space.corner", SettingsScope.Space, "Appearance", "Visual", "Corner style", "Rounded or square corners for this space.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Rounded", "Square" }),
        new("space.icon.layout", SettingsScope.Space, "Appearance", "Icon Layout", "Icon alignment", "Grid, list, or free layout within this space.", SettingValueKind.Choice,
            new[] { "Grid", "List", "Free" }),
        new("space.title.font", SettingsScope.Space, "Appearance", "Typography", "Title font", "Per-space title font face.", SettingValueKind.Text),
        new("space.title.color", SettingsScope.Space, "Appearance", "Typography", "Title font color", "Per-space title text color.", SettingValueKind.Color),

        new("space.rollup.mode", SettingsScope.Space, "Behavior", "Roll-Up", "Roll-up mode", "Click, hover, disabled, or use global mode for this space.", SettingValueKind.Choice,
            new[] { "UseGlobal", "ClickToOpen", "HoverToOpen", "Disabled" }),
        new("space.quickHide.mode", SettingsScope.Space, "Behavior", "Visibility", "Quick-hide behavior", "Space-specific quick-hide exceptions and reveal mode.", SettingValueKind.Choice,
            new[] { "UseGlobal", "ExcludeFromQuickHide", "ShowOnDemand" }),
        new("space.position.mode", SettingsScope.Space, "Behavior", "Positioning", "Position mode", "Snap-to-grid or free positioning for this space.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Snap", "Free" }),
        new("space.portal.mode", SettingsScope.Space, "Behavior", "Portal", "Portal behavior", "Enable portal behavior if this space maps to a folder.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Portal", "Standard" }),
        new("space.sort.mode", SettingsScope.Space, "Behavior", "Sorting", "Sort mode", "Name, type, modified date, usage, or custom rule ordering.", SettingValueKind.Choice,
            new[] { "Manual", "Name", "Type", "DateModified", "Usage", "Custom" }),

        new("space.rules.targets", SettingsScope.Space, "Rules", "Auto-Placement", "Target rules", "Item path and extension patterns routed into this space.", SettingValueKind.Text),
        new("space.rules.exclude", SettingsScope.Space, "Rules", "Auto-Placement", "Exclusion rules", "File types or shortcuts excluded from this space ruleset.", SettingValueKind.Text),

        new("space.locked", SettingsScope.Space, "Advanced", "Safety", "Lock space", "Prevent accidental move/resize operations.", SettingValueKind.Toggle),
        new("space.perMonitor", SettingsScope.Space, "Advanced", "Monitor", "Per-monitor state", "Store space behavior override by monitor identity.", SettingValueKind.Toggle),
        new("space.doubleClickDesktop", SettingsScope.Space, "Advanced", "Pages", "Desktop double-click jump", "Double-click desktop to jump to this space/page.", SettingValueKind.Toggle),
    };

    public static IReadOnlyList<SettingDefinition> Search(string query)
    {
        if (string.IsNullOrWhiteSpace(query))
            return GlobalSettings.Concat(SpaceSettings).ToList();

        string q = query.Trim();
        return GlobalSettings
            .Concat(SpaceSettings)
            .Where(s => Contains(s.Key, q)
                        || Contains(s.Label, q)
                        || Contains(s.Tooltip, q)
                        || Contains(s.Section, q)
                        || Contains(s.Tab, q))
            .ToList();
    }

    private static bool Contains(string value, string query) =>
        value.Contains(query, StringComparison.OrdinalIgnoreCase);
}
