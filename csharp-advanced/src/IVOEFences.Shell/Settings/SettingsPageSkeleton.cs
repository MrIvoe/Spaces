using System;
using System.Collections.Generic;
using System.Linq;

namespace IVOEFences.Shell.Settings;

internal enum SettingsTabId
{
    Global,
    PerFence,
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
            BuildPerFenceTab(),
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
                        new SettingsControlSkeleton("global.rollup.enabled", SettingsControlType.Checkbox, "Enable roll-up for all fences", "Enable click or hover roll-up interactions.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.rollup.mode", SettingsControlType.RadioGroup, "Roll-up mode", "Choose how fences open when rolled up.", new[] { "Click to open", "Hover to open", "Always expanded" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.titlebar.show", SettingsControlType.Checkbox, "Show titlebars", "Show fence titlebars on all fences.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.titlebar.mouseover", SettingsControlType.Checkbox, "Mouseover titlebars only", "Show titlebars only while hovering.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.peek", SettingsControlType.Checkbox, "Keep fences on top (Peek)", "Temporarily elevate fences above normal desktop clutter.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.blur", SettingsControlType.Checkbox, "Enable blur behind fences", "Apply blur/acrylic where supported.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.opacity", SettingsControlType.Slider, "Global fence transparency", "Adjust default fence transparency.", Min: 0, Max: 100, SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "global.desktop",
                    "Desktop & Multi-Monitor",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.snap.grid", SettingsControlType.Checkbox, "Snap fences to grid", "Align fences to a configurable desktop grid.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("global.monitor.layouts", SettingsControlType.Checkbox, "Store layouts per monitor config", "Persist layout profiles by monitor topology."),
                        new SettingsControlSkeleton("global.monitor.swap", SettingsControlType.Button, "Swap screen contents", "Fix misplaced fences/icons after monitor changes."),
                        new SettingsControlSkeleton("global.spacing", SettingsControlType.Slider, "Inter-fence spacing", "Minimum spacing between nearby fences.", Min: 0, Max: 50, SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "global.portals",
                    "Folder Portals",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("global.portal.enabled", SettingsControlType.Checkbox, "Enable folder portals", "Show folder contents directly inside fences."),
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
                        new SettingsControlSkeleton("global.quickHide.exclude", SettingsControlType.Checkbox, "Exclude icons not in fences from QuickHide", "Prevent unmanaged icons from being hidden."),
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
                        new SettingsControlSkeleton("global.layouts.backup", SettingsControlType.Button, "Backup layouts", "Create a snapshot backup of all fences."),
                        new SettingsControlSkeleton("global.layouts.restore", SettingsControlType.Button, "Restore layouts", "Restore fences from a snapshot backup."),
                        new SettingsControlSkeleton("global.dll.forceUnload", SettingsControlType.Checkbox, "Force-unload DLL after startup", "Compatibility option for shell startup behavior."),
                    }),
            });
    }

    private static SettingsTabSkeleton BuildPerFenceTab()
    {
        return new SettingsTabSkeleton(
            SettingsTabId.PerFence,
            "Per-Fence Settings",
            new[]
            {
                new SettingsSectionSkeleton(
                    "fence.selector",
                    "Fence Selection",
                    IsCollapsible: false,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("fence.selector", SettingsControlType.ComboBox, "Select fence", "Choose the fence to configure."),
                    }),

                new SettingsSectionSkeleton(
                    "fence.appearance",
                    "Appearance",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("fence.titlebar.show", SettingsControlType.Checkbox, "Show titlebar", "Override global titlebar visibility.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.titlebar.mode", SettingsControlType.RadioGroup, "Titlebar mode", "Choose visible, hidden, or mouseover-only titlebar.", new[] { "Visible", "Hidden", "Mouseover only" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.color", SettingsControlType.ColorPicker, "Fence color", "Set a per-fence background color.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.opacity", SettingsControlType.Slider, "Opacity", "Set per-fence opacity.", Min: 0, Max: 100, SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.font", SettingsControlType.FontPicker, "Title font", "Set per-fence title font.", SupportsLivePreview: true),
                    }),

                new SettingsSectionSkeleton(
                    "fence.behavior",
                    "Behavior",
                    IsCollapsible: true,
                    DefaultCollapsed: false,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("fence.rollup.mode", SettingsControlType.RadioGroup, "Roll-up mode", "Fence-specific roll-up behavior.", new[] { "Click to open", "Hover to open", "Always expanded" }, SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.quickHide", SettingsControlType.Checkbox, "QuickHide icons", "Control icon visibility behavior for this fence.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.lock", SettingsControlType.Checkbox, "Lock fence position", "Prevent move and resize operations."),
                        new SettingsControlSkeleton("fence.snap.grid", SettingsControlType.Checkbox, "Snap to grid", "Enable grid alignment for this fence.", SupportsLivePreview: true),
                        new SettingsControlSkeleton("fence.portal.enabled", SettingsControlType.Checkbox, "Enable folder portal", "Treat this fence as a portal to a folder."),
                    }),

                new SettingsSectionSkeleton(
                    "fence.rules",
                    "Rules",
                    IsCollapsible: true,
                    DefaultCollapsed: true,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("fence.rules.manage", SettingsControlType.Button, "Manage rules", "Create target-based and name-based rules for this fence."),
                    }),

                new SettingsSectionSkeleton(
                    "fence.advanced",
                    "Advanced",
                    IsCollapsible: true,
                    DefaultCollapsed: true,
                    Controls: new[]
                    {
                        new SettingsControlSkeleton("fence.perMonitor", SettingsControlType.Checkbox, "Store settings per monitor", "Persist this fence behavior per monitor identity."),
                        new SettingsControlSkeleton("fence.livePreview", SettingsControlType.Checkbox, "Enable live preview", "Apply appearance and behavior changes immediately.", SupportsLivePreview: true),
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
                        new SettingsControlSkeleton("advanced.reset", SettingsControlType.Button, "Reset all fences", "Reset fence layout and behavior to defaults."),
                        new SettingsControlSkeleton("advanced.reload", SettingsControlType.Button, "Force reload layouts", "Reload fence windows from persisted state."),
                        new SettingsControlSkeleton("advanced.logging", SettingsControlType.Checkbox, "Enable advanced logging", "Enable deep runtime traces and diagnostics."),
                    }),
            });
    }
}
