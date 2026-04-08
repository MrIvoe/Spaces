using System;
using System.Collections.Generic;
using System.Linq;

namespace IVOESpaces.Shell.Settings;

internal enum SettingsTabId
{
    Global,
    PerSpace,
    Advanced,
}

internal enum SettingsControlType
{
    Checkbox,
    RadioGroup,
    Slider,
    Button,
    ComboBox,
    ColorPicker,
    FontPicker,
}

internal sealed record SettingsControlSkeleton(
    string Id,
    SettingsControlType Type,
    string Label,
    string Tooltip,
    string[]? Options = null,
    int? Min = null,
    int? Max = null,
    bool SupportsLivePreview = false);

internal sealed record SettingsSectionSkeleton(
    string Id,
    string Title,
    bool IsCollapsible,
    bool DefaultCollapsed,
    IReadOnlyList<SettingsControlSkeleton> Controls);

internal sealed record SettingsTabSkeleton(
    SettingsTabId Id,
    string Title,
    IReadOnlyList<SettingsSectionSkeleton> Sections);

internal sealed record SettingsPageSkeleton(IReadOnlyList<SettingsTabSkeleton> Tabs)
{
    public IReadOnlyList<SettingsControlSkeleton> SearchControls(string query)
    {
        if (string.IsNullOrWhiteSpace(query))
            return Tabs.SelectMany(t => t.Sections).SelectMany(s => s.Controls).ToList();

        string q = query.Trim();
        return Tabs
            .SelectMany(t => t.Sections)
            .SelectMany(s => s.Controls)
            .Where(c => Contains(c.Id, q)
                        || Contains(c.Label, q)
                        || Contains(c.Tooltip, q)
                        || (c.Options != null && c.Options.Any(o => Contains(o, q))))
            .ToList();
    }

    private static bool Contains(string value, string query) =>
        value.Contains(query, StringComparison.OrdinalIgnoreCase);
}

internal static class SettingsPageSkeletonFactory
{
    public static SettingsPageSkeleton CreateDefault()
    {
        return new SettingsPageSkeleton(new[]
        {
            BuildGlobalTab(),
            BuildPerSpaceTab(),
            BuildAdvancedTab(),
        });
    }

    private static SettingsTabSkeleton BuildGlobalTab()
    {
        return new SettingsTabSkeleton(
            SettingsTabId.Global,
            "Global Settings",
            new[]
            {
                new SettingsSectionSkeleton(
                    "global.display",
                    "Display & Behavior",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.rollup.enabled", SettingsControlType.Checkbox, "Enable roll-up for all spaces", "Enable click or hover roll-up interactions.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.rollup.mode", SettingsControlType.RadioGroup, "Roll-up mode", "Choose how spaces open when rolled up.", new[] { "Click to open", "Hover to open", "Always expanded" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.titlebar.show", SettingsControlType.Checkbox, "Show titlebars", "Show space titlebars on all spaces.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.titlebar.mouseover", SettingsControlType.Checkbox, "Mouseover titlebars only", "Show titlebars only while hovering.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.peek", SettingsControlType.Checkbox, "Keep spaces on top (Peek)", "Temporarily elevate spaces above normal desktop clutter.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.blur", SettingsControlType.Checkbox, "Enable blur behind spaces", "Apply blur/acrylic where supported.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.opacity", SettingsControlType.Slider, "Global space transparency", "Adjust default space transparency.", Min: 0, Max: 100, SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "global.desktop",
                    "Desktop & Multi-Monitor",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.snap.grid", SettingsControlType.Checkbox, "Snap spaces to grid", "Align spaces to a configurable desktop grid.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.monitor.layouts", SettingsControlType.Checkbox, "Store layouts per monitor config", "Persist layout profiles by monitor topology."),
                        new SettingsControlSkeleton("global.monitor.swap", SettingsControlType.Button, "Swap screen contents", "Fix misplaced spaces/icons after monitor changes."),
                        new SettingsControlSkeleton("global.spacing", SettingsControlType.Slider, "Inter-space spacing", "Minimum spacing between nearby spaces.", Min: 0, Max: 50, SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "global.portals",
                    "Folder Portals",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.portal.enabled", SettingsControlType.Checkbox, "Enable folder portals", "Show folder contents directly inside spaces."),
                        new SettingsControlSkeleton("global.portal.separateExplorer", SettingsControlType.Checkbox, "Open in separate Explorer process", "Launch portal navigation in a dedicated explorer.exe process."),
                        new SettingsControlSkeleton("global.portal.smoothScroll", SettingsControlType.Checkbox, "Smooth scroll folder portals", "Scroll portal content with finer granularity."),
                        new SettingsControlSkeleton("global.portal.resizeSpeed", SettingsControlType.Slider, "Portal resize speed", "Tune redraw speed for portal resize operations.", Min: 1, Max: 10, SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "global.rules",
                    "Rules & Organization",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.rules.apply", SettingsControlType.Checkbox, "Apply rules to all icons", "Automatically place icons based on global rules."),
                        new SettingsControlSkeleton("global.quickHide.exclude", SettingsControlType.Checkbox, "Exclude icons not in spaces from QuickHide", "Prevent unmanaged icons from being hidden."),
                        new SettingsControlSkeleton("global.hotkeys.manage", SettingsControlType.Button, "Manage hotkeys", "Configure Win+D, Win+Space, and custom shortcuts."),
                        new SettingsControlSkeleton("global.localization", SettingsControlType.Button, "Language / localization settings", "Configure language and locale behavior."),
                    }),

                new SettingsSectionSkeleton(
                    "global.advanced",
                    "Advanced / Debug",
                    IsCollapsible: true,
                    DefaultCollapsed: true,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.debug.logging", SettingsControlType.Checkbox, "Enable debug logging", "Write verbose diagnostic output for troubleshooting."),
                        new SettingsControlSkeleton("global.layouts.backup", SettingsControlType.Button, "Backup layouts", "Create a snapshot backup of all spaces."),
                        new SettingsControlSkeleton("global.layouts.restore", SettingsControlType.Button, "Restore layouts", "Restore spaces from a snapshot backup."),
                        new SettingsControlSkeleton("global.dll.forceUnload", SettingsControlType.Checkbox, "Force-unload DLL after startup", "Compatibility option for shell startup behavior."),
                    }),
            });
    }

    private static SettingsTabSkeleton BuildPerSpaceTab()
    {
        return new SettingsTabSkeleton(
            SettingsTabId.PerSpace,
            "Per-Space Settings",
            new[]
            {
                new SettingsSectionSkeleton(
                    "space.selector",
                    "Space Selection",
                    IsCollapsible: false,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("space.selector", SettingsControlType.ComboBox, "Select space", "Choose the space to configure."),
                    }),

                new SettingsSectionSkeleton(
                    "space.appearance",
                    "Appearance",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("space.titlebar.show", SettingsControlType.Checkbox, "Show titlebar", "Override global titlebar visibility.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.titlebar.mode", SettingsControlType.RadioGroup, "Titlebar mode", "Choose visible, hidden, or mouseover-only titlebar.", new[] { "Visible", "Hidden", "Mouseover only" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.color", SettingsControlType.ColorPicker, "Space color", "Set a per-space background color.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.opacity", SettingsControlType.Slider, "Opacity", "Set per-space opacity.", Min: 0, Max: 100, SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.font", SettingsControlType.FontPicker, "Title font", "Set per-space title font.", SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "space.behavior",
                    "Behavior",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("space.rollup.mode", SettingsControlType.RadioGroup, "Roll-up mode", "Space-specific roll-up behavior.", new[] { "Click to open", "Hover to open", "Always expanded" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.quickHide", SettingsControlType.Checkbox, "QuickHide icons", "Control icon visibility behavior for this space.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.lock", SettingsControlType.Checkbox, "Lock space position", "Prevent move and resize operations."),
                        new SettingsControlSkeleton("space.snap.grid", SettingsControlType.Checkbox, "Snap to grid", "Enable grid alignment for this space.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("space.portal.enabled", SettingsControlType.Checkbox, "Enable folder portal", "Treat this space as a portal to a folder."),
                    }),

                new SettingsSectionSkeleton(
                    "space.rules",
                    "Rules",
                    IsCollapsible: true,
                    DefaultCollapsed: true,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("space.rules.manage", SettingsControlType.Button, "Manage rules", "Create target-based and name-based rules for this space."),
                    }),

                new SettingsSectionSkeleton(
                    "space.advanced",
                    "Advanced",
                    IsCollapsible: true,
                    DefaultCollapsed: true,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("space.perMonitor", SettingsControlType.Checkbox, "Store settings per monitor", "Persist this space behavior per monitor identity."),
                        new SettingsControlSkeleton("space.livePreview", SettingsControlType.Checkbox, "Enable live preview", "Apply appearance and behavior changes immediately.", SupportsLivePreview: true),
                    }),
            });
    }

    private static SettingsTabSkeleton BuildAdvancedTab()
    {
        return new SettingsTabSkeleton(
            SettingsTabId.Advanced,
            "Advanced / Troubleshooting",
            new[]
            {
                new SettingsSectionSkeleton(
                    "advanced.tools",
                    "Troubleshooting Tools",
                    IsCollapsible: false,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("advanced.debug.package", SettingsControlType.Button, "Create debug package", "Bundle logs and settings for diagnostics."),
                        new SettingsControlSkeleton("advanced.reset", SettingsControlType.Button, "Reset all spaces", "Reset space layout and behavior to defaults."),
                        new SettingsControlSkeleton("advanced.reload", SettingsControlType.Button, "Force reload layouts", "Reload space windows from persisted state."),
                        new SettingsControlSkeleton("advanced.logging", SettingsControlType.Checkbox, "Enable advanced logging", "Enable deep runtime traces and diagnostics."),
                    }),
            });
    }
}
