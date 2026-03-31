using System;
using System.Collections.Generic;
using System.Linq;

namespace IVOEFences.Shell.Settings;

internal enum SettingsScope
{
    Global,
    Fence,
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
        new("global.rollup.mode", SettingsScope.Global, "Display", "Behavior", "Global roll-up mode", "Controls how all fences expand: click, hover, or always expanded.", SettingValueKind.Choice,
            new[] { "ClickToOpen", "HoverToOpen", "AlwaysExpanded" }),
        new("global.titlebar.mode", SettingsScope.Global, "Display", "Behavior", "Global titlebar mode", "Show titlebars always, only on mouseover, or hide them.", SettingValueKind.Choice,
            new[] { "Visible", "Mouseover", "Hidden" }),
        new("global.peek.keepOnTop", SettingsScope.Global, "Display", "Behavior", "Peek mode keep-on-top", "Temporarily keep fences visible above desktop clutter during peek interactions.", SettingValueKind.Toggle),
        new("global.blur.enabled", SettingsScope.Global, "Display", "Appearance", "Blur behind fences", "Enable acrylic/blur treatment behind fences where supported.", SettingValueKind.Toggle),
        new("global.hidpi.perMonitor", SettingsScope.Global, "Display", "Appearance", "Per-monitor DPI scaling", "Scale titlebars and fence internals for each monitor DPI.", SettingValueKind.Toggle),

        new("global.snap.edges", SettingsScope.Global, "Desktop", "Snap", "Snap to screen edges", "Snap fences to monitor bounds while dragging.", SettingValueKind.Toggle),
        new("global.snap.grid", SettingsScope.Global, "Desktop", "Snap", "Snap to grid", "Align fence movement and resize to grid intervals.", SettingValueKind.Toggle),
        new("global.spacing", SettingsScope.Global, "Desktop", "Snap", "Inter-fence spacing", "Minimum spacing between fences when snapping.", SettingValueKind.Number),
        new("global.dragdrop.standardMode", SettingsScope.Global, "Desktop", "Drag & Drop", "Standard fence drop behavior", "Choose whether dropped items are referenced, moved into fence storage, or added as shortcuts.", SettingValueKind.Choice,
            new[] { "Reference", "MoveIntoFenceStorage", "Shortcut" }),
        new("global.dragdrop.confirmExternal", SettingsScope.Global, "Desktop", "Drag & Drop", "Confirm external drops", "Prompt before importing files dragged from outside the desktop.", SettingValueKind.Toggle),
        new("global.dragdrop.applyRules", SettingsScope.Global, "Desktop", "Drag & Drop", "Apply rules after drop", "Run placement rules immediately after accepting a drop.", SettingValueKind.Toggle),
        new("global.dragdrop.highlightTargets", SettingsScope.Global, "Desktop", "Drag & Drop", "Highlight drop targets", "Show fence highlight while a drag operation is hovering.", SettingValueKind.Toggle),
        new("global.monitor.detectChanges", SettingsScope.Global, "Desktop", "Multi-Monitor", "Detect monitor layout changes", "Track monitor topology changes and activate matching layout profile.", SettingValueKind.Toggle),
        new("global.monitor.autoSwap", SettingsScope.Global, "Desktop", "Multi-Monitor", "Auto-swap misplaced groups", "Try to recover misplaced fence groups after monitor sleep/reconnect.", SettingValueKind.Toggle),

        new("global.portal.enabled", SettingsScope.Global, "Folder Portals", "Integration", "Enable folder portals", "Allow fences to render live folder contents.", SettingValueKind.Toggle),
        new("global.portal.separateExplorer", SettingsScope.Global, "Folder Portals", "Integration", "Open folders in separate Explorer process", "Launch portal targets in dedicated explorer.exe process.", SettingValueKind.Toggle),
        new("global.portal.scrollbar", SettingsScope.Global, "Folder Portals", "Performance", "Portal scrollbar behavior", "Control portal scrollbar mode for compact and large content views.", SettingValueKind.Choice,
            new[] { "Auto", "AlwaysVisible", "Hidden" }),
        new("global.portal.fastResize", SettingsScope.Global, "Folder Portals", "Performance", "Fast portal resize", "Prioritize portal redraw speed during resize operations.", SettingValueKind.Toggle),

        new("global.rules.enabled", SettingsScope.Global, "Rules", "Organization", "Enable global placement rules", "Apply rule-based auto-placement to new desktop items.", SettingValueKind.Toggle),
        new("global.quickHide.enabled", SettingsScope.Global, "Rules", "Organization", "Quick-hide unmanaged icons", "Hide desktop items that are not assigned to a fence.", SettingValueKind.Toggle),
        new("global.hotkeys.enabled", SettingsScope.Global, "Hotkeys", "Keyboard", "Enable global hotkeys", "Master switch for keyboard shortcuts.", SettingValueKind.Toggle),
        new("global.language", SettingsScope.Global, "General", "Localization", "Language", "UI language/localization pack identifier.", SettingValueKind.Text),

        new("global.logging.verbose", SettingsScope.Global, "Advanced", "Diagnostics", "Advanced logging", "Enable verbose diagnostics and recovery traces.", SettingValueKind.Toggle),
        new("global.snapshot.backup", SettingsScope.Global, "Advanced", "Recovery", "Backup layout", "Create a full fence snapshot.", SettingValueKind.Action),
        new("global.snapshot.restore", SettingsScope.Global, "Advanced", "Recovery", "Restore layout", "Restore a previously saved snapshot.", SettingValueKind.Action),
        new("global.forceUnloadDll", SettingsScope.Global, "Advanced", "Compatibility", "Force-unload shell DLL after startup", "Aggressive compatibility toggle for shell extension startup behavior.", SettingValueKind.Toggle, IsBeta: true),
        new("global.uac.resolve", SettingsScope.Global, "Advanced", "Compatibility", "UAC prompt mitigation", "Use safer process launch behavior to minimize repeated UAC prompts.", SettingValueKind.Toggle),
    };

    public static IReadOnlyList<SettingDefinition> FenceSettings { get; } = new List<SettingDefinition>
    {
        new("fence.titlebar.mode", SettingsScope.Fence, "Appearance", "Titlebar", "Titlebar mode", "Visible, hidden, or mouseover-only titlebar behavior for this fence.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Visible", "Mouseover", "Hidden" }),
        new("fence.color.background", SettingsScope.Fence, "Appearance", "Visual", "Background color", "Optional per-fence background color override.", SettingValueKind.Color),
        new("fence.opacity", SettingsScope.Fence, "Appearance", "Visual", "Transparency", "Optional per-fence opacity override.", SettingValueKind.Number),
        new("fence.blur.enabled", SettingsScope.Fence, "Appearance", "Visual", "Blur", "Enable/disable blur for this fence.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Enabled", "Disabled" }),
        new("fence.corner", SettingsScope.Fence, "Appearance", "Visual", "Corner style", "Rounded or square corners for this fence.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Rounded", "Square" }),
        new("fence.icon.layout", SettingsScope.Fence, "Appearance", "Icon Layout", "Icon alignment", "Grid, list, or free layout within this fence.", SettingValueKind.Choice,
            new[] { "Grid", "List", "Free" }),
        new("fence.title.font", SettingsScope.Fence, "Appearance", "Typography", "Title font", "Per-fence title font face.", SettingValueKind.Text),
        new("fence.title.color", SettingsScope.Fence, "Appearance", "Typography", "Title font color", "Per-fence title text color.", SettingValueKind.Color),

        new("fence.rollup.mode", SettingsScope.Fence, "Behavior", "Roll-Up", "Roll-up mode", "Click, hover, disabled, or use global mode for this fence.", SettingValueKind.Choice,
            new[] { "UseGlobal", "ClickToOpen", "HoverToOpen", "Disabled" }),
        new("fence.quickHide.mode", SettingsScope.Fence, "Behavior", "Visibility", "Quick-hide behavior", "Fence-specific quick-hide exceptions and reveal mode.", SettingValueKind.Choice,
            new[] { "UseGlobal", "ExcludeFromQuickHide", "ShowOnDemand" }),
        new("fence.position.mode", SettingsScope.Fence, "Behavior", "Positioning", "Position mode", "Snap-to-grid or free positioning for this fence.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Snap", "Free" }),
        new("fence.portal.mode", SettingsScope.Fence, "Behavior", "Portal", "Portal behavior", "Enable portal behavior if this fence maps to a folder.", SettingValueKind.Choice,
            new[] { "UseGlobal", "Portal", "Standard" }),
        new("fence.sort.mode", SettingsScope.Fence, "Behavior", "Sorting", "Sort mode", "Name, type, modified date, usage, or custom rule ordering.", SettingValueKind.Choice,
            new[] { "Manual", "Name", "Type", "DateModified", "Usage", "Custom" }),

        new("fence.rules.targets", SettingsScope.Fence, "Rules", "Auto-Placement", "Target rules", "Item path and extension patterns routed into this fence.", SettingValueKind.Text),
        new("fence.rules.exclude", SettingsScope.Fence, "Rules", "Auto-Placement", "Exclusion rules", "File types or shortcuts excluded from this fence ruleset.", SettingValueKind.Text),

        new("fence.locked", SettingsScope.Fence, "Advanced", "Safety", "Lock fence", "Prevent accidental move/resize operations.", SettingValueKind.Toggle),
        new("fence.perMonitor", SettingsScope.Fence, "Advanced", "Monitor", "Per-monitor state", "Store fence behavior override by monitor identity.", SettingValueKind.Toggle),
        new("fence.doubleClickDesktop", SettingsScope.Fence, "Advanced", "Pages", "Desktop double-click jump", "Double-click desktop to jump to this fence/page.", SettingValueKind.Toggle),
    };

    public static IReadOnlyList<SettingDefinition> Search(string query)
    {
        if (string.IsNullOrWhiteSpace(query))
            return GlobalSettings.Concat(FenceSettings).ToList();

        string q = query.Trim();
        return GlobalSettings
            .Concat(FenceSettings)
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
