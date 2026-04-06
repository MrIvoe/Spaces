using System.Runtime.InteropServices;
using System.Linq;
using IVOEFences.Core;
using IVOEFences.Core.Models;
using IVOEFences.Core.Plugins;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Native;
using IVOEFences.Shell.UI;
using Serilog;

namespace IVOEFences.Shell.Settings;

/// <summary>
/// Settings window — a standard Win32 dialog-style window.
/// Not parented to WorkerW; appears on top of everything as a normal app window.
/// Singleton: calling ShowOrFocus() twice focuses the existing window.
/// Loads from / saves to <see cref="AppSettingsRepository"/>.
/// </summary>
internal sealed class SettingsWindow
{
    private const string ClassName = AppIdentity.SettingsWindowClass;
    private const int Width = 760;
    private const int Height = 620;
    private const int HeaderHeight = 58;
    private const int TabHeight = 36;
    private const int LeftNavWidth = 236;
    private const int ContentTop = 78;
    private const int RowHeight = 34;
    private const uint WmAutoApply = Win32.WM_USER + 210;
    private const int AutoApplyDelayMs = 120;

    private static SettingsWindow? _instance;
    private static Win32.WndProc?  _wndProcDelegate; // keep delegate alive (prevent GC)
    private static bool            _classRegistered;
    private static readonly string[] SortModes =
    {
        FenceWindow.FenceSortMode.Manual.ToString(),
        FenceWindow.FenceSortMode.Name.ToString(),
        FenceWindow.FenceSortMode.Type.ToString(),
        FenceWindow.FenceSortMode.DateModified.ToString(),
        FenceWindow.FenceSortMode.Usage.ToString(),
    };

    private static readonly int[] IconSizes = { 16, 32, 48, 64, 96 };
    private static readonly int[] OpacityLevels = { 60, 70, 80, 85, 90, 100 };
    private static readonly int[] PeekDelayOptions = { 250, 500, 750, 1000, 1500 };
    private static readonly int[] SpacingOptions = { 0, 8, 12, 16, 24, 32, 48, 64 };
    private static readonly int[] IconSpacingOptions = { 2, 4, 6, 8, 10, 12, 14, 16, 20 };
    private static readonly int[] GlassStrengthOptions = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
    private static readonly string[] ThemeOptions = { "Auto", "Light", "Dark", "Accent" };
    private static readonly string[] GlobalTitlebarModes = { "Visible", "Mouseover", "Hidden" };
    private static readonly string[] GlobalRollupModes = { "ClickToOpen", "HoverToOpen", "AlwaysExpanded" };
    private static readonly string[] PerFenceTitlebarModes = { "UseGlobal", "Visible", "Mouseover", "Hidden" };
    private static readonly string[] PerFenceRollupModes = { "UseGlobal", "ClickToOpen", "HoverToOpen", "AlwaysExpanded", "Disabled" };
    private static readonly string[] PerFenceSnapModes = { "UseGlobal", "Snap", "Free" };
    private static readonly string[] PerFencePortalModes = { "UseGlobal", "Enabled", "Disabled" };
    private static readonly string[] PerFenceBlurModes = { "UseGlobal", "Enabled", "Disabled" };
    private static readonly string[] LiveFolderViews = { "Icons", "List", "Details" };
    private static readonly string[] StandardFenceDropModes = { "Reference", "MoveIntoFenceStorage", "Shortcut" };
    private static readonly string[] TrayLeftClickActions = { "OpenSettings", "ToggleFences", "ShowMenu" };

    // ── Live mutable settings (edited in-window, committed on Save) ────────
    private bool _showFencesOnStartup;
    private bool _hideDesktopOutsideFences;
    private bool _autoArrangeOnStartup;
    private bool _useAiDefaultMode;
    private bool _startWithWindows;
    private bool _enableGlobalHotkeys;
    private int _peekDelayMs;
    private string _toggleHotkey = "Win+Space";
    private string _searchHotkey = "Win+F";
    private string _defaultSortMode = FenceWindow.FenceSortMode.Name.ToString();
    private int _iconSize;
    private int _iconSpacing;
    private bool _autoResizeFencesOnIconSizeChange;
    private bool _showFenceTitles;
    private int _fenceOpacity;
    private bool _blurBackground;
    private int _glassStrength;
    private bool _enableAnimations;
    private bool _snapToScreenEdges;
    private bool _snapToOtherFences;
    private bool _snapToGrid;
    private int _interFenceSpacing;
    private bool _enableFolderPortalNavigation;
    private bool _liveFolderShowIcon;
    private string _liveFolderDefaultView = "Icons";
    private bool _enableGlobalPlacementRules;
    private bool _enableQuickHideMode;
    private bool _rollupRequiresClick;
    private bool _rollupRequiresHover;
    private bool _enableDesktopPages;
    private int _currentDesktopPage;
    private int _desktopPageCount;
    private string _globalTitlebarMode = "Visible";
    private string _globalRollupMode = "ClickToOpen";
    private string _themeMode = "Auto";
    private string _standardFenceDropMode = "Reference";
    private bool _confirmExternalDrops;
    private bool _autoApplyRulesOnDrop = true;
    private bool _highlightDropTargets = true;
    private string _trayLeftClickAction = "OpenSettings";

    private Tab _activeTab = Tab.General;
    private Guid? _selectedFenceId;
    private string _statusText = string.Empty;

    private readonly FenceManager _manager;
    private readonly List<PluginSettingDefinition> _pluginSettingDefinitions;
    private readonly IReadOnlyList<SettingDefinition> _pluginSettingBlueprints;
    private readonly List<HitTarget> _hitTargets = new();
    private readonly SettingsPageSkeleton _skeleton;

    private IntPtr _hwnd = IntPtr.Zero;
    private readonly object _autoApplyLock = new();
    private System.Threading.Timer? _autoApplyDebounceTimer;
    private int _scrollOffsetY;
    private bool _hasPendingChanges;

    private enum Tab
    {
        General,
        Hotkeys,
        Fences,
        Rules,
        DesktopPages,
        Plugins,
        Advanced,
    }

    private sealed record HitTarget(Win32.RECT Rect, Action Action);

    // ── Public API ──────────────────────────────────────────────────────────

    public static void ShowOrFocus(FenceManager manager, IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        if (_instance == null || !Win32.IsWindow(_instance._hwnd))
        {
            _instance = new SettingsWindow(manager, pluginSettings);
            _instance.LoadFromSettings();
            _instance.Create();
        }
        else
        {
            Log.Information("SettingsWindow: focusing existing window HWND={H:X}", _instance._hwnd);
            Win32.SetForegroundWindow(_instance._hwnd);
            Win32.ShowWindow(_instance._hwnd, Win32.SW_SHOW);
            Win32.ShowWindow(_instance._hwnd, Win32.SW_RESTORE);
        }
    }

    private SettingsWindow(FenceManager manager, IReadOnlyList<PluginSettingDefinition> pluginSettings)
    {
        _manager = manager;
        _pluginSettingDefinitions = pluginSettings.ToList();
        _pluginSettingBlueprints = PluginSettingsBlueprintAdapter.Convert(_pluginSettingDefinitions);
        _skeleton = SettingsPageSkeletonFactory.CreateDefault();
    }

    // ── Load / Save ───────────────────────────────────────────────────────

    private void LoadFromSettings()
    {
        var s = AppSettingsRepository.Instance.Current;
        _showFencesOnStartup = s.ShowFencesOnStartup;
        _hideDesktopOutsideFences = s.HideDesktopIconsOutsideFences;
        _autoArrangeOnStartup = s.AutoArrangeOnStartup;
        _useAiDefaultMode = s.UseAiDefaultMode;
        _startWithWindows = s.StartWithWindows;
        _enableGlobalHotkeys = s.EnableGlobalHotkeys;
        _peekDelayMs = s.PeekDelayMs;
        _toggleHotkey = s.ToggleHotkey;
        _searchHotkey = s.SearchHotkey;
        _defaultSortMode = s.DefaultSortMode;
        _iconSize = s.IconSize;
        _iconSpacing = Math.Clamp(s.IconSpacing, 2, 20);
        _autoResizeFencesOnIconSizeChange = s.AutoResizeFencesOnIconSizeChange;
        _showFenceTitles = s.ShowFenceTitles;
        _fenceOpacity = s.FenceOpacity;
        _blurBackground = s.BlurBackground;
        _glassStrength = Math.Clamp(s.GlassStrength, 0, 100);
        _enableAnimations = s.EnableAnimations;
        _snapToScreenEdges = s.SnapToScreenEdges;
        _snapToOtherFences = s.SnapToOtherFences;
        _snapToGrid = s.SnapToGrid;
        _interFenceSpacing = Math.Clamp(s.InterFenceSpacing, 0, 64);
        _enableFolderPortalNavigation = s.EnableFolderPortalNavigation;
        _liveFolderShowIcon = s.LiveFolderShowIcon;
        _liveFolderDefaultView = string.IsNullOrWhiteSpace(s.LiveFolderDefaultView)
            ? "Icons"
            : s.LiveFolderDefaultView;
        _enableGlobalPlacementRules = s.EnableGlobalPlacementRules;
        _enableQuickHideMode = s.EnableQuickHideMode;
        _rollupRequiresClick = s.RollupRequiresClick;
        _rollupRequiresHover = s.RollupRequiresHover;
        _enableDesktopPages = s.EnableDesktopPages;
        _currentDesktopPage = Math.Max(0, s.CurrentDesktopPage);
        _desktopPageCount = Math.Max(1, s.DesktopPageCount);
        _globalTitlebarMode = s.GlobalTitlebarMode;
        _globalRollupMode = _rollupRequiresHover
            ? "HoverToOpen"
            : (_rollupRequiresClick ? "ClickToOpen" : s.GlobalRollupMode);
        _themeMode = NormalizeThemeMode(s.ThemeMode);
        _standardFenceDropMode = string.IsNullOrWhiteSpace(s.StandardFenceDropMode) ? "Reference" : s.StandardFenceDropMode;
        _confirmExternalDrops = s.ConfirmExternalDrops;
        _autoApplyRulesOnDrop = s.AutoApplyRulesOnDrop;
        _highlightDropTargets = s.HighlightDropTargets;
        _trayLeftClickAction = string.IsNullOrWhiteSpace(s.TrayLeftClickAction) ? "OpenSettings" : s.TrayLeftClickAction;
        ApplyThemeModeToEngine();
    }

    private void CommitToSettings()
    {
        var current = AppSettingsRepository.Instance.Current;
        int nextIconSpacing = Math.Clamp(_iconSpacing, 2, 20);
        int nextGlassStrength = Math.Clamp(_glassStrength, 0, 100);
        int nextInterFenceSpacing = Math.Clamp(_interFenceSpacing, 0, 64);
        int nextCurrentDesktopPage = Math.Max(0, _currentDesktopPage);
        int nextDesktopPageCount = Math.Max(1, _desktopPageCount);
        string nextGlobalRollupMode = _rollupRequiresHover
            ? "HoverToOpen"
            : (_rollupRequiresClick ? "ClickToOpen" : _globalRollupMode);
        string nextThemeMode = NormalizeThemeMode(_themeMode);

        var changedKeys = new List<string>();

        if (current.ShowFencesOnStartup != _showFencesOnStartup) changedKeys.Add(nameof(AppSettings.ShowFencesOnStartup));
        if (current.HideDesktopIconsOutsideFences != _hideDesktopOutsideFences) changedKeys.Add(nameof(AppSettings.HideDesktopIconsOutsideFences));
        if (current.AutoArrangeOnStartup != _autoArrangeOnStartup) changedKeys.Add(nameof(AppSettings.AutoArrangeOnStartup));
        if (current.UseAiDefaultMode != _useAiDefaultMode) changedKeys.Add(nameof(AppSettings.UseAiDefaultMode));
        if (current.StartWithWindows != _startWithWindows) changedKeys.Add(nameof(AppSettings.StartWithWindows));
        if (current.EnableGlobalHotkeys != _enableGlobalHotkeys) changedKeys.Add(nameof(AppSettings.EnableGlobalHotkeys));
        if (current.PeekDelayMs != _peekDelayMs) changedKeys.Add(nameof(AppSettings.PeekDelayMs));
        if (!string.Equals(current.ToggleHotkey, _toggleHotkey, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.ToggleHotkey));
        if (!string.Equals(current.SearchHotkey, _searchHotkey, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.SearchHotkey));
        if (!string.Equals(current.DefaultSortMode, _defaultSortMode, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.DefaultSortMode));
        if (current.IconSize != _iconSize) changedKeys.Add(nameof(AppSettings.IconSize));
        if (current.IconSpacing != nextIconSpacing) changedKeys.Add(nameof(AppSettings.IconSpacing));
        if (current.AutoResizeFencesOnIconSizeChange != _autoResizeFencesOnIconSizeChange) changedKeys.Add(nameof(AppSettings.AutoResizeFencesOnIconSizeChange));
        if (current.ShowFenceTitles != _showFenceTitles) changedKeys.Add(nameof(AppSettings.ShowFenceTitles));
        if (current.FenceOpacity != _fenceOpacity) changedKeys.Add(nameof(AppSettings.FenceOpacity));
        if (current.BlurBackground != _blurBackground) changedKeys.Add(nameof(AppSettings.BlurBackground));
        if (current.GlassStrength != nextGlassStrength) changedKeys.Add(nameof(AppSettings.GlassStrength));
        if (current.EnableAnimations != _enableAnimations) changedKeys.Add(nameof(AppSettings.EnableAnimations));
        if (current.SnapToScreenEdges != _snapToScreenEdges) changedKeys.Add(nameof(AppSettings.SnapToScreenEdges));
        if (current.SnapToOtherFences != _snapToOtherFences) changedKeys.Add(nameof(AppSettings.SnapToOtherFences));
        if (current.SnapToGrid != _snapToGrid) changedKeys.Add(nameof(AppSettings.SnapToGrid));
        if (current.InterFenceSpacing != nextInterFenceSpacing) changedKeys.Add(nameof(AppSettings.InterFenceSpacing));
        if (current.EnableFolderPortalNavigation != _enableFolderPortalNavigation) changedKeys.Add(nameof(AppSettings.EnableFolderPortalNavigation));
        if (current.LiveFolderShowIcon != _liveFolderShowIcon) changedKeys.Add(nameof(AppSettings.LiveFolderShowIcon));
        if (!string.Equals(current.LiveFolderDefaultView, _liveFolderDefaultView, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.LiveFolderDefaultView));
        if (current.EnableGlobalPlacementRules != _enableGlobalPlacementRules) changedKeys.Add(nameof(AppSettings.EnableGlobalPlacementRules));
        if (current.EnableQuickHideMode != _enableQuickHideMode) changedKeys.Add(nameof(AppSettings.EnableQuickHideMode));
        if (current.RollupRequiresClick != _rollupRequiresClick) changedKeys.Add(nameof(AppSettings.RollupRequiresClick));
        if (current.RollupRequiresHover != _rollupRequiresHover) changedKeys.Add(nameof(AppSettings.RollupRequiresHover));
        if (current.EnableDesktopPages != _enableDesktopPages) changedKeys.Add(nameof(AppSettings.EnableDesktopPages));
        if (current.CurrentDesktopPage != nextCurrentDesktopPage) changedKeys.Add(nameof(AppSettings.CurrentDesktopPage));
        if (current.DesktopPageCount != nextDesktopPageCount) changedKeys.Add(nameof(AppSettings.DesktopPageCount));
        if (!string.Equals(current.GlobalTitlebarMode, _globalTitlebarMode, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.GlobalTitlebarMode));
        if (!string.Equals(current.GlobalRollupMode, nextGlobalRollupMode, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.GlobalRollupMode));
        if (!string.Equals(current.ThemeMode, nextThemeMode, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.ThemeMode));
        if (!string.Equals(current.StandardFenceDropMode, _standardFenceDropMode, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.StandardFenceDropMode));
        if (current.ConfirmExternalDrops != _confirmExternalDrops) changedKeys.Add(nameof(AppSettings.ConfirmExternalDrops));
        if (current.AutoApplyRulesOnDrop != _autoApplyRulesOnDrop) changedKeys.Add(nameof(AppSettings.AutoApplyRulesOnDrop));
        if (current.HighlightDropTargets != _highlightDropTargets) changedKeys.Add(nameof(AppSettings.HighlightDropTargets));
        if (!string.Equals(current.TrayLeftClickAction, _trayLeftClickAction, StringComparison.Ordinal)) changedKeys.Add(nameof(AppSettings.TrayLeftClickAction));

        SettingsManager.Instance.Update(s =>
        {
            s.ShowFencesOnStartup = _showFencesOnStartup;
            s.HideDesktopIconsOutsideFences = _hideDesktopOutsideFences;
            s.AutoArrangeOnStartup = _autoArrangeOnStartup;
            s.UseAiDefaultMode = _useAiDefaultMode;
            s.StartWithWindows = _startWithWindows;
            s.EnableGlobalHotkeys = _enableGlobalHotkeys;
            s.PeekDelayMs = _peekDelayMs;
            s.ToggleHotkey = _toggleHotkey;
            s.SearchHotkey = _searchHotkey;
            s.DefaultSortMode = _defaultSortMode;
            s.IconSize = _iconSize;
            s.IconSpacing = nextIconSpacing;
            s.AutoResizeFencesOnIconSizeChange = _autoResizeFencesOnIconSizeChange;
            s.ShowFenceTitles = _showFenceTitles;
            s.FenceOpacity = _fenceOpacity;
            s.BlurBackground = _blurBackground;
            s.GlassStrength = nextGlassStrength;
            s.EnableAnimations = _enableAnimations;
            s.SnapToScreenEdges = _snapToScreenEdges;
            s.SnapToOtherFences = _snapToOtherFences;
            s.SnapToGrid = _snapToGrid;
            s.InterFenceSpacing = nextInterFenceSpacing;
            s.EnableFolderPortalNavigation = _enableFolderPortalNavigation;
            s.LiveFolderShowIcon = _liveFolderShowIcon;
            s.LiveFolderDefaultView = _liveFolderDefaultView;
            s.EnableGlobalPlacementRules = _enableGlobalPlacementRules;
            s.EnableQuickHideMode = _enableQuickHideMode;
            s.RollupRequiresClick = _rollupRequiresClick;
            s.RollupRequiresHover = _rollupRequiresHover;
            s.EnableDesktopPages = _enableDesktopPages;
            s.CurrentDesktopPage = nextCurrentDesktopPage;
            s.DesktopPageCount = nextDesktopPageCount;
            s.GlobalTitlebarMode = _globalTitlebarMode;
            s.GlobalRollupMode = nextGlobalRollupMode;
            s.ThemeMode = nextThemeMode;
            s.StandardFenceDropMode = _standardFenceDropMode;
            s.ConfirmExternalDrops = _confirmExternalDrops;
            s.AutoApplyRulesOnDrop = _autoApplyRulesOnDrop;
            s.HighlightDropTargets = _highlightDropTargets;
            s.TrayLeftClickAction = _trayLeftClickAction;
        }, changedKeys.ToArray());
        ApplyThemeModeToEngine();
        Log.Information("SettingsWindow: settings saved");
    }

    private void ApplyChanges(bool closeWindow)
    {
        CommitToSettings();
        _hasPendingChanges = false;
        ScheduleAutoApply();
        _statusText = "Settings applied";

        if (closeWindow)
        {
            Win32.DestroyWindow(_hwnd);
            return;
        }

        Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
    }

    private void ApplyChangesRealtime()
    {
        ApplyThemeModeToEngine();
        PreviewFenceSettings();
        Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
    }

    /// <summary>
    /// Apply a mutation immediately to local draft state and schedule debounced previews.
    /// </summary>
    private void ApplyImmediate(Action mutate, string status = "Unsaved changes")
    {
        mutate();
        _hasPendingChanges = true;
        _statusText = status;
        ScheduleAutoApply();
        Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
    }

    private void CancelChangesAndClose()
    {
        if (_hasPendingChanges)
        {
            LoadFromSettings();
            ApplyChangesRealtime();
        }

        Win32.DestroyWindow(_hwnd);
    }

    private void ScheduleAutoApply()
    {
        lock (_autoApplyLock)
        {
            _autoApplyDebounceTimer?.Dispose();
            _autoApplyDebounceTimer = new System.Threading.Timer(
                _ =>
                {
                    if (_hwnd != IntPtr.Zero && Win32.IsWindow(_hwnd))
                        Win32.PostMessage(_hwnd, WmAutoApply, IntPtr.Zero, IntPtr.Zero);
                },
                null,
                AutoApplyDelayMs,
                Timeout.Infinite);
        }
    }

    // ── Creation ────────────────────────────────────────────────────────────

    private void Create()
    {
        RegisterClassOnce();

        // Center on primary monitor
        int screenW = Win32.GetSystemMetrics(Win32.SM_CXSCREEN);
        int screenH = Win32.GetSystemMetrics(Win32.SM_CYSCREEN);
        int x = (screenW - Width)  / 2;
        int y = (screenH - Height) / 2;

        _hwnd = Win32.CreateWindowEx(
            Win32.WS_EX_APPWINDOW,          // appears in taskbar
            ClassName,
            AppIdentity.SettingsWindowTitle,
            Win32.WS_OVERLAPPED
                | Win32.WS_CAPTION
                | Win32.WS_SYSMENU
                | Win32.WS_VISIBLE,         // visible immediately
            x, y, Width, Height,
            IntPtr.Zero,                    // no parent — free-floating window
            IntPtr.Zero,
            Win32.GetModuleHandle(null),
            IntPtr.Zero);

        if (_hwnd == IntPtr.Zero)
        {
            Log.Error("SettingsWindow: CreateWindowEx failed (err {E})",
                Marshal.GetLastWin32Error());
            return;
        }

        Win32.ShowWindow(_hwnd, Win32.SW_SHOW);
        Win32.SetForegroundWindow(_hwnd);
        Win32.UpdateWindow(_hwnd);
        Log.Information("SettingsWindow: created HWND={H:X}", _hwnd);
    }

    private static void RegisterClassOnce()
    {
        if (_classRegistered) return;

        _wndProcDelegate = WndProc;

        var wc = new Win32.WNDCLASSEX
        {
            cbSize        = (uint)Marshal.SizeOf<Win32.WNDCLASSEX>(),
            style         = Win32.CS_HREDRAW | Win32.CS_VREDRAW,
            lpfnWndProc   = Marshal.GetFunctionPointerForDelegate<Win32.WndProc>(_wndProcDelegate),
            hInstance     = Win32.GetModuleHandle(null),
            hCursor       = Win32.LoadCursor(IntPtr.Zero, Win32.IDC_ARROW),
            hbrBackground = IntPtr.Zero, // we own all painting in WM_PAINT
            lpszClassName = ClassName,
        };

        if (!Win32.RegisterClassEx(ref wc))
        {
            int err = Marshal.GetLastWin32Error();
            if (err != 1410) // ERROR_CLASS_ALREADY_EXISTS is fine
                Log.Error("SettingsWindow: RegisterClassEx failed (err {E})", err);
        }

        _classRegistered = true;
    }

    // ── Window procedure ────────────────────────────────────────────────────

    private static IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        switch (msg)
        {
            case Win32.WM_PAINT:
                if (_instance != null) _instance.Paint(hwnd);
                return IntPtr.Zero;

            case Win32.WM_ERASEBKGND:
                return new IntPtr(1); // handled in WM_PAINT

            case Win32.WM_LBUTTONDOWN:
            {
                if (_instance == null) break;
                int mx = Win32.GET_X_LPARAM(lParam);
                int my = Win32.GET_Y_LPARAM(lParam);
                _instance.HandleClick(hwnd, mx, my);
                return IntPtr.Zero;
            }

            case WmAutoApply:
                _instance?.ApplyChangesRealtime();
                return IntPtr.Zero;

            case Win32.WM_MOUSEWHEEL:
            {
                if (_instance == null) break;
                int delta = (short)(wParam.ToInt64() >> 16);
                _instance._scrollOffsetY = Math.Max(0, _instance._scrollOffsetY - delta / 4);
                Win32.InvalidateRect(hwnd, IntPtr.Zero, true);
                return IntPtr.Zero;
            }

            case Win32.WM_KEYDOWN:
            {
                int vk = wParam.ToInt32();
                if (vk == Win32.VK_ESCAPE)
                {
                    _instance?.CancelChangesAndClose();
                    return IntPtr.Zero;
                }
                break;
            }

            case Win32.WM_CLOSE:
                _instance?.CancelChangesAndClose();
                return IntPtr.Zero;

            case Win32.WM_DESTROY:
                if (_instance != null)
                {
                    lock (_instance._autoApplyLock)
                    {
                        _instance._autoApplyDebounceTimer?.Dispose();
                        _instance._autoApplyDebounceTimer = null;
                    }
                }
                _instance = null;
                return IntPtr.Zero;
        }

        return Win32.DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // ── Click handling ─────────────────────────────────────────────────────

    private void HandleClick(IntPtr hwnd, int mx, int my)
    {
        foreach (HitTarget target in _hitTargets.AsEnumerable().Reverse())
        {
            if (Contains(target.Rect, mx, my))
            {
                target.Action();
                Win32.InvalidateRect(hwnd, IntPtr.Zero, true);
                return;
            }
        }
    }

    private static bool Contains(Win32.RECT rect, int x, int y)
    {
        return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
    }

    private void AddHitTarget(Win32.RECT rect, Action action)
    {
        _hitTargets.Add(new HitTarget(rect, action));
    }

    private FenceWindow? GetSelectedFence()
    {
        List<FenceWindow> windows = _manager.Windows.ToList();
        if (windows.Count == 0)
            return null;

        FenceWindow? selected = null;
        if (_selectedFenceId.HasValue)
            selected = windows.FirstOrDefault(w => w.ModelId == _selectedFenceId.Value);

        if (selected == null)
        {
            selected = windows[0];
            _selectedFenceId = selected.ModelId;
        }

        return selected;
    }

    private static string NextString(string current, string[] options)
    {
        if (options.Length == 0)
            return current;

        int index = Array.FindIndex(options, o => string.Equals(o, current, StringComparison.OrdinalIgnoreCase));
        if (index < 0)
            return options[0];
        return options[(index + 1) % options.Length];
    }

    private static int NextInt(int current, int[] options)
    {
        if (options.Length == 0)
            return current;

        int index = Array.IndexOf(options, current);
        if (index < 0)
            return options[0];
        return options[(index + 1) % options.Length];
    }

    private void UpdateAndPreview(Action mutate)
    {
        mutate();
        _hasPendingChanges = true;
        _statusText = "Unsaved changes";
        ScheduleAutoApply();
    }

    private void PreviewFenceSettings()
    {
        foreach (FenceWindow window in _manager.Windows)
        {
            // Only repaint windows whose preview state actually differs
            if (!window.LivePreviewDiffers(
                    _globalTitlebarMode,
                    _globalRollupMode,
                    _snapToGrid,
                    _enableFolderPortalNavigation,
                    _fenceOpacity,
                    _blurBackground,
                    _glassStrength,
                    _iconSpacing))
                continue;

            window.ApplyLivePreview(
                _globalTitlebarMode,
                _globalRollupMode,
                _snapToGrid,
                _enableFolderPortalNavigation,
                _fenceOpacity,
                _blurBackground,
                _glassStrength,
                _iconSpacing);
        }
    }

    // ── Theme-aware color helpers ────────────────────────────────────────────

    private static string NormalizeThemeMode(string? themeMode)
    {
        if (string.IsNullOrWhiteSpace(themeMode))
            return "Auto";

        foreach (string option in ThemeOptions)
        {
            if (string.Equals(themeMode, option, StringComparison.OrdinalIgnoreCase))
                return option;
        }

        return "Auto";
    }

    private static ThemeEngine.ThemeMode ToEngineThemeMode(string themeMode)
    {
        return themeMode switch
        {
            "Light" => ThemeEngine.ThemeMode.Light,
            "Dark" => ThemeEngine.ThemeMode.Dark,
            _ => ThemeEngine.ThemeMode.Auto,
        };
    }

    private void ApplyThemeModeToEngine()
    {
        string normalized = NormalizeThemeMode(_themeMode);
        _themeMode = normalized;
        ThemeEngine.Instance.CurrentThemeMode = ToEngineThemeMode(normalized);
    }

    private bool IsAccentThemeSelected() =>
        string.Equals(_themeMode, "Accent", StringComparison.OrdinalIgnoreCase);

    private bool IsSettingsDarkMode()
    {
        if (IsAccentThemeSelected())
            return ThemeEngine.Instance.DetectDarkMode();

        return ThemeEngine.Instance.IsDarkMode;
    }

    private static uint ColorToColorRef(System.Drawing.Color color) =>
        Win32.RGB(color.R, color.G, color.B);

    // Color selectors — all painting goes through these.
    private static uint ClrBg(bool dark)         => dark ? Win32.RGB(28, 28, 30)    : Win32.RGB(244, 246, 249);
    private static uint ClrSurface(bool dark)    => dark ? Win32.RGB(40, 40, 44)    : Win32.RGB(255, 255, 255);
    private static uint ClrSurfaceAlt(bool dark) => dark ? Win32.RGB(50, 50, 56)    : Win32.RGB(245, 247, 250);
    private uint ClrHeader(bool dark)
    {
        if (IsAccentThemeSelected())
            return ColorToColorRef(ThemeEngine.Instance.AccentColor);

        return dark ? Win32.RGB(20, 70, 120) : Win32.RGB(26, 99, 156);
    }
    private static uint ClrTabActive(bool dark)  => dark ? Win32.RGB(40, 40, 44)    : Win32.RGB(255, 255, 255);
    private static uint ClrTabInactive(bool dark)=> dark ? Win32.RGB(30, 30, 34)    : Win32.RGB(221, 228, 236);
    private static uint ClrText(bool dark)       => dark ? Win32.RGB(220, 220, 220) : Win32.RGB(35, 35, 35);
    private static uint ClrTextMuted(bool dark)  => dark ? Win32.RGB(140, 140, 148) : Win32.RGB(90, 90, 110);
    private static uint ClrAccent(bool dark)     => dark ? Win32.RGB(99, 165, 240)  : Win32.RGB(26, 99, 156);
    private static uint ClrBorder(bool dark)     => dark ? Win32.RGB(60, 60, 66)    : Win32.RGB(210, 215, 222);
    private static uint ClrTabText(bool dark, bool selected) =>
        selected ? ClrAccent(dark) : (dark ? Win32.RGB(175, 175, 185) : Win32.RGB(60, 76, 92));

    // ── GDI painting ────────────────────────────────────────────────────────

    private void Paint(IntPtr hwnd)
    {
        IntPtr hdc = Win32.BeginPaint(hwnd, out Win32.PAINTSTRUCT ps);
        Win32.GetClientRect(hwnd, out Win32.RECT client);
        bool dark = IsSettingsDarkMode();

        _hitTargets.Clear();

        // Background
        IntPtr bgBrush = Win32.CreateSolidBrush(ClrBg(dark));
        Win32.FillRect(hdc, ref client, bgBrush);
        Win32.DeleteObject(bgBrush);

        // Header band
        var header = new Win32.RECT { left = 0, top = 0, right = client.right, bottom = HeaderHeight };
        IntPtr headerBrush = Win32.CreateSolidBrush(ClrHeader(dark));
        Win32.FillRect(hdc, ref header, headerBrush);
        Win32.DeleteObject(headerBrush);

        IntPtr titleFont = Win32.CreateFont(
            20, 0, 0, 0, Win32.FW_BOLD, 0, 0, 0,
            (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
            (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
            (uint)Win32.DEFAULT_PITCH, "Segoe UI");
        IntPtr oldFont = Win32.SelectObject(hdc, titleFont);
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, Win32.RGB(255, 255, 255));
        var titleRect = new Win32.RECT { left = 18, top = 12, right = client.right - 20, bottom = 48 };
        Win32.DrawText(hdc, AppIdentity.SettingsWindowTitle, -1, ref titleRect,
            Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
        Win32.SelectObject(hdc, oldFont);
        Win32.DeleteObject(titleFont);

        IntPtr bodyFont = Win32.CreateFont(
            14, 0, 0, 0, Win32.FW_NORMAL, 0, 0, 0,
            (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
            (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
            (uint)Win32.DEFAULT_PITCH, "Segoe UI");
        IntPtr semiboldFont = Win32.CreateFont(
            14, 0, 0, 0, 600, 0, 0, 0,
            (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
            (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
            (uint)Win32.DEFAULT_PITCH, "Segoe UI");

        DrawTabs(hdc, client.right, client.bottom, dark);

        IntPtr oldBody = Win32.SelectObject(hdc, bodyFont);
        switch (_activeTab)
        {
            case Tab.General:
                DrawGeneralTab(hdc, client.right, dark);
                break;
            case Tab.Hotkeys:
                DrawHotkeysTab(hdc, client.right, dark);
                break;
            case Tab.Fences:
                DrawFencesTab(hdc, semiboldFont, client.right, dark);
                break;
            case Tab.Rules:
                DrawRulesTab(hdc, client.right, dark);
                break;
            case Tab.DesktopPages:
                DrawDesktopPagesTab(hdc, client.right, dark);
                break;
            case Tab.Plugins:
                DrawPluginsTab(hdc, client.right, dark);
                break;
            case Tab.Advanced:
                DrawAdvancedTab(hdc, client.right, dark);
                break;
        }
        Win32.SelectObject(hdc, oldBody);

        DrawBottomButtons(hdc, semiboldFont, client.right, client.bottom, dark);

        Win32.DeleteObject(bodyFont);
        Win32.DeleteObject(semiboldFont);
        Win32.EndPaint(hwnd, ref ps);
    }

    private static int ContentLeft => LeftNavWidth + 26;

    private static int ContentRight(int clientRight) => clientRight - 22;

    private int ScrolledContentTop => ContentTop - _scrollOffsetY;

    private static int RowLabelLeft => ContentLeft + 12;

    private static int RowValueRight(int clientRight) => ContentRight(clientRight) - 12;

    private static int RowValueLeft(int clientRight) => ContentRight(clientRight) - 194;

    private void DrawSectionHeader(IntPtr hdc, string title, int y, int clientRight, bool dark)
    {
        var sectionRect = new Win32.RECT
        {
            left = ContentLeft,
            top = y,
            right = ContentRight(clientRight),
            bottom = y + 24,
        };

        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, ClrTextMuted(dark));
        Win32.DrawText(hdc, title, -1, ref sectionRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

        var line = new Win32.RECT
        {
            left = sectionRect.left,
            top = sectionRect.bottom - 1,
            right = sectionRect.right,
            bottom = sectionRect.bottom,
        };
        IntPtr brush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref line, brush);
        Win32.DeleteObject(brush);
    }

    private void DrawTabs(IntPtr hdc, int clientRight, int clientBottom, bool dark)
    {
        string[] labels = { "General", "Hotkeys", "Fences", "Rules", "Pages", "Plugins", "Advanced" };
        int navTop = HeaderHeight + 10;
        int navBottom = clientBottom - 62;
        int navHeight = Math.Max(1, navBottom - navTop);
        int tabStride = Math.Max(TabHeight + 10, navHeight / labels.Length);

        var railRect = new Win32.RECT
        {
            left = 14,
            top = navTop - 6,
            right = LeftNavWidth,
            bottom = navBottom,
        };
        IntPtr railBrush = Win32.CreateSolidBrush(ClrSurfaceAlt(dark));
        Win32.FillRect(hdc, ref railRect, railBrush);
        Win32.DeleteObject(railBrush);

        var divider = new Win32.RECT
        {
            left = LeftNavWidth,
            top = HeaderHeight,
            right = LeftNavWidth + 1,
            bottom = navBottom,
        };
        IntPtr dividerBrush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref divider, dividerBrush);
        Win32.DeleteObject(dividerBrush);

        for (int i = 0; i < labels.Length; i++)
        {
            var tabRect = new Win32.RECT
            {
                left = 20,
                top = navTop + i * tabStride,
                right = LeftNavWidth - 10,
                bottom = navTop + i * tabStride + TabHeight,
            };

            bool selected = (int)_activeTab == i;
            IntPtr tabBrush = Win32.CreateSolidBrush(selected ? ClrTabActive(dark) : ClrTabInactive(dark));
            Win32.FillRect(hdc, ref tabRect, tabBrush);
            Win32.DeleteObject(tabBrush);

            // Left accent block for selected tab
            if (selected)
            {
                var accent = new Win32.RECT { left = tabRect.left, top = tabRect.top, right = tabRect.left + 4, bottom = tabRect.bottom };
                IntPtr accentBrush = Win32.CreateSolidBrush(ClrAccent(dark));
                Win32.FillRect(hdc, ref accent, accentBrush);
                Win32.DeleteObject(accentBrush);
            }

            Win32.SetBkMode(hdc, Win32.TRANSPARENT);
            Win32.SetTextColor(hdc, ClrTabText(dark, selected));
            FluentIcons.DrawGlyph(hdc, GetTabIcon((Tab)i), tabRect.left + 10, tabRect.top + 8, 16,
                ClrTabText(dark, selected));
            var txt = new Win32.RECT
            {
                left = tabRect.left + 34,
                top = tabRect.top,
                right = tabRect.right - 10,
                bottom = tabRect.bottom,
            };
            Win32.DrawText(hdc, labels[i], -1, ref txt, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

            Tab targetTab = (Tab)i;
            AddHitTarget(tabRect, () => { _activeTab = targetTab; _scrollOffsetY = 0; });
        }
    }

    private static FluentIcon GetTabIcon(Tab tab)
    {
        return tab switch
        {
            Tab.General => FluentIcon.Apps,
            Tab.Hotkeys => FluentIcon.Keyboard,
            Tab.Fences => FluentIcon.Grid,
            Tab.Rules => FluentIcon.Flow,
            Tab.DesktopPages => FluentIcon.Desktop,
            Tab.Plugins => FluentIcon.PlugConnected,
            _ => FluentIcon.Wrench,
        };
    }

    private void DrawBottomButtons(IntPtr hdc, IntPtr semiboldFont, int clientRight, int clientBottom, bool dark)
    {
        int y = clientBottom - 52;

        // Separator above buttons
        var sepRect = new Win32.RECT { left = 0, top = y - 8, right = clientRight, bottom = y - 7 };
        IntPtr sepBrush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref sepRect, sepBrush);
        Win32.DeleteObject(sepBrush);

        DrawButton(hdc, semiboldFont,
            new Win32.RECT { left = clientRight - 338, top = y, right = clientRight - 242, bottom = y + 34 },
            "Cancel",
            dark ? Win32.RGB(70, 74, 80) : Win32.RGB(228, 231, 238),
            ClrText(dark),
            CancelChangesAndClose, dark);

        DrawButton(hdc, semiboldFont,
            new Win32.RECT { left = clientRight - 232, top = y, right = clientRight - 136, bottom = y + 34 },
            "Apply",
            dark ? Win32.RGB(38, 95, 68) : Win32.RGB(198, 230, 214),
            dark ? Win32.RGB(230, 245, 236) : Win32.RGB(22, 74, 51),
            () => ApplyChanges(closeWindow: false), dark);

        DrawButton(hdc, semiboldFont,
            new Win32.RECT { left = clientRight - 126, top = y, right = clientRight - 20, bottom = y + 34 },
            "Save && Close",
            ClrHeader(dark),
            Win32.RGB(255, 255, 255),
            () => ApplyChanges(closeWindow: true), dark);

        if (!string.IsNullOrWhiteSpace(_statusText))
        {
            var statusRect = new Win32.RECT
            {
                left = 20,
                top = y,
                right = clientRight - 370,
                bottom = y + 34,
            };
            Win32.SetTextColor(hdc, dark ? Win32.RGB(80, 210, 120) : Win32.RGB(28, 117, 54));
            Win32.DrawText(hdc, _statusText, -1, ref statusRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
        }

        if (_hasPendingChanges)
        {
            var pendingRect = new Win32.RECT
            {
                left = clientRight - 470,
                top = y,
                right = clientRight - 350,
                bottom = y + 34,
            };
            Win32.SetTextColor(hdc, dark ? Win32.RGB(255, 194, 94) : Win32.RGB(142, 84, 0));
            Win32.DrawText(hdc, "Unsaved", -1, ref pendingRect, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
        }
    }

    private void DrawButton(IntPtr hdc, IntPtr font, Win32.RECT rect, string label, uint bgColor, uint fgColor, Action onClick, bool dark)
    {
        IntPtr old = Win32.SelectObject(hdc, font);
        IntPtr brush = Win32.CreateSolidBrush(bgColor);
        Win32.FillRect(hdc, ref rect, brush);
        Win32.DeleteObject(brush);

        // Subtle border around button
        IntPtr borderPen = Win32.CreatePen(Win32.PS_SOLID, 1, ClrBorder(dark));
        IntPtr oldPen = Win32.SelectObject(hdc, borderPen);
        IntPtr nullBrush = Win32.GetStockObject(Win32.NULL_BRUSH);
        IntPtr oldBrush = Win32.SelectObject(hdc, nullBrush);
        Win32.RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 4, 4);
        Win32.SelectObject(hdc, oldBrush);
        Win32.SelectObject(hdc, oldPen);
        Win32.DeleteObject(borderPen);

        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, fgColor);
        Win32.DrawText(hdc, label, -1, ref rect, Win32.DT_CENTER | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
        Win32.SelectObject(hdc, old);
        AddHitTarget(rect, onClick);
    }

    private int DrawBoolRow(IntPtr hdc, int y, int clientRight, string label, bool value, Action onToggle, bool dark, bool trackSettingChange = true)
    {
        var row = new Win32.RECT { left = ContentLeft, top = y, right = ContentRight(clientRight), bottom = y + RowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        // Bottom separator
        var sep = new Win32.RECT { left = ContentLeft, top = y + RowHeight, right = ContentRight(clientRight), bottom = y + RowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = RowLabelLeft, top = y, right = RowValueLeft(clientRight) - 12, bottom = y + RowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        // Toggle switch visual
        var box = new Win32.RECT { left = RowValueRight(clientRight) - 42, top = y + 9, right = RowValueRight(clientRight), bottom = y + 25 };
        uint switchBg = value ? ClrAccent(dark) : (dark ? Win32.RGB(70, 70, 76) : Win32.RGB(180, 185, 195));
        IntPtr switchBrush = Win32.CreateSolidBrush(switchBg);
        Win32.FillRect(hdc, ref box, switchBrush);
        Win32.DeleteObject(switchBrush);

        // Thumb
        int thumbX = value ? (box.right - 14) : (box.left + 2);
        var thumb = new Win32.RECT { left = thumbX, top = box.top + 2, right = thumbX + 12, bottom = box.bottom - 2 };
        IntPtr thumbBrush = Win32.CreateSolidBrush(Win32.RGB(255, 255, 255));
        Win32.FillRect(hdc, ref thumb, thumbBrush);
        Win32.DeleteObject(thumbBrush);

        AddHitTarget(row, () =>
        {
            if (trackSettingChange)
                ApplyImmediate(onToggle);
            else
                onToggle();
        });
        return y + RowHeight + 1;
    }

    private int DrawChoiceRow(IntPtr hdc, int y, int clientRight, string label, string value, Action onCycle, bool dark, bool trackSettingChange = true)
    {
        var row = new Win32.RECT { left = ContentLeft, top = y, right = ContentRight(clientRight), bottom = y + RowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        var sep = new Win32.RECT { left = ContentLeft, top = y + RowHeight, right = ContentRight(clientRight), bottom = y + RowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = RowLabelLeft, top = y, right = RowValueLeft(clientRight) - 8, bottom = y + RowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        var valRect = new Win32.RECT { left = RowValueLeft(clientRight), top = y, right = RowValueRight(clientRight), bottom = y + RowHeight };
        Win32.SetTextColor(hdc, ClrAccent(dark));
        Win32.DrawText(hdc, value, -1, ref valRect, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);
        FluentIcons.DrawGlyph(hdc, FluentIcon.ChevronRight, RowValueRight(clientRight) - 18, y + 8, 14, ClrAccent(dark));

        AddHitTarget(row, () =>
        {
            if (trackSettingChange)
                ApplyImmediate(onCycle);
            else
                onCycle();
        });
        return y + RowHeight + 1;
    }

    private int DrawActionRow(IntPtr hdc, int y, int clientRight, string label, string actionLabel, Action action, bool dark)
    {
        var row = new Win32.RECT { left = ContentLeft, top = y, right = ContentRight(clientRight), bottom = y + RowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        var sep = new Win32.RECT { left = ContentLeft, top = y + RowHeight, right = ContentRight(clientRight), bottom = y + RowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = RowLabelLeft, top = y, right = RowValueLeft(clientRight) - 8, bottom = y + RowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        var actionRect = new Win32.RECT { left = RowValueLeft(clientRight), top = y, right = RowValueRight(clientRight), bottom = y + RowHeight };
        Win32.SetTextColor(hdc, ClrAccent(dark));
        Win32.DrawText(hdc, actionLabel, -1, ref actionRect, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);
        FluentIcons.DrawGlyph(hdc, FluentIcon.ChevronRight, RowValueRight(clientRight) - 18, y + 8, 14, ClrAccent(dark));

        AddHitTarget(row, action);
        return y + RowHeight + 1;
    }

    private void DrawGeneralTab(IntPtr hdc, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;
        DrawSectionHeader(hdc, "Startup", y - 26, clientRight, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Show fences on startup", _showFencesOnStartup, () => _showFencesOnStartup = !_showFencesOnStartup, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Start with Windows", _startWithWindows, () => _startWithWindows = !_startWithWindows, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Auto-arrange desktop icons on first run", _autoArrangeOnStartup, () => _autoArrangeOnStartup = !_autoArrangeOnStartup, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Use AI grouping by default for auto-arrange", _useAiDefaultMode, () => _useAiDefaultMode = !_useAiDefaultMode, dark);
        DrawSectionHeader(hdc, "Desktop", y + 8, clientRight, dark);
        y += 30;
        y = DrawBoolRow(hdc, y, clientRight, "Hide desktop originals while fenced", _hideDesktopOutsideFences, () => _hideDesktopOutsideFences = !_hideDesktopOutsideFences, dark);
        DrawSectionHeader(hdc, "Tray", y + 8, clientRight, dark);
        y += 30;
        _ = DrawChoiceRow(hdc, y, clientRight, "Tray icon left-click action", _trayLeftClickAction, () =>
            _trayLeftClickAction = NextString(_trayLeftClickAction, TrayLeftClickActions), dark);
    }

    private void DrawHotkeysTab(IntPtr hdc, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;
        DrawSectionHeader(hdc, "Global Shortcuts", y - 26, clientRight, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Enable global hotkeys", _enableGlobalHotkeys, () => _enableGlobalHotkeys = !_enableGlobalHotkeys, dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Toggle fences hotkey", _toggleHotkey, () => _toggleHotkey = NextString(_toggleHotkey, new[] { "Win+Space", "Ctrl+Win+Space", "Ctrl+Win+T" }), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Search hotkey", _searchHotkey, () => _searchHotkey = NextString(_searchHotkey, new[] { "Win+F", "Ctrl+Win+F", "Ctrl+Win+K" }), dark);
        _ = DrawChoiceRow(hdc, y, clientRight, "Peek delay", $"{_peekDelayMs} ms", () => _peekDelayMs = NextInt(_peekDelayMs, PeekDelayOptions), dark);
    }

    private void DrawFencesTab(IntPtr hdc, IntPtr semiboldFont, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;
        DrawSectionHeader(hdc, "Global Fence Defaults", y - 26, clientRight, dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Default sort mode", _defaultSortMode, () => UpdateAndPreview(() =>
            _defaultSortMode = NextString(_defaultSortMode, SortModes)), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Default icon size", $"{_iconSize}px", () => UpdateAndPreview(() =>
            _iconSize = NextInt(_iconSize, IconSizes)), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Auto-resize fences when icon size changes", _autoResizeFencesOnIconSizeChange,
            () => UpdateAndPreview(() => _autoResizeFencesOnIconSizeChange = !_autoResizeFencesOnIconSizeChange), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Global titlebar mode", _globalTitlebarMode, () => UpdateAndPreview(() =>
            _globalTitlebarMode = NextString(_globalTitlebarMode, GlobalTitlebarModes)), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Global roll-up mode", _globalRollupMode, () => UpdateAndPreview(() =>
            _globalRollupMode = NextString(_globalRollupMode, GlobalRollupModes)), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Require click to open rollup fences", _rollupRequiresClick,
            () => UpdateAndPreview(() =>
            {
                _rollupRequiresClick = !_rollupRequiresClick;
                if (_rollupRequiresClick)
                    _rollupRequiresHover = false;
            }), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Require hover to open rollup fences", _rollupRequiresHover,
            () => UpdateAndPreview(() =>
            {
                _rollupRequiresHover = !_rollupRequiresHover;
                if (_rollupRequiresHover)
                    _rollupRequiresClick = false;
            }), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Show fence titles", _showFenceTitles, () => UpdateAndPreview(() =>
            _showFenceTitles = !_showFenceTitles), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Fence opacity", $"{_fenceOpacity}%", () => UpdateAndPreview(() =>
            _fenceOpacity = NextInt(_fenceOpacity, OpacityLevels)), dark);

        Win32.SelectObject(hdc, semiboldFont);
        var sectionTitle = new Win32.RECT { left = ContentLeft, top = y + 6, right = ContentRight(clientRight), bottom = y + 32 };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, ClrTextMuted(dark));
        Win32.DrawText(hdc, "Per-fence controls", -1, ref sectionTitle, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

        int listTop = y + 34;
        int listHeight = 140;
        var listRect = new Win32.RECT { left = ContentLeft, top = listTop, right = ContentRight(clientRight), bottom = listTop + listHeight };
        IntPtr listBrush = Win32.CreateSolidBrush(ClrSurface(dark));
        Win32.FillRect(hdc, ref listRect, listBrush);
        Win32.DeleteObject(listBrush);

        List<FenceWindow> windows = _manager.Windows.ToList();
        if (windows.Count == 0)
        {
            var emptyRect = new Win32.RECT { left = RowLabelLeft, top = listTop + 10, right = ContentRight(clientRight) - 12, bottom = listTop + 36 };
            Win32.SetTextColor(hdc, ClrTextMuted(dark));
            Win32.DrawText(hdc, "No fences are currently active", -1, ref emptyRect, Win32.DT_LEFT | Win32.DT_SINGLELINE | Win32.DT_VCENTER);
            return;
        }

        if (!_selectedFenceId.HasValue || windows.All(w => w.ModelId != _selectedFenceId.Value))
            _selectedFenceId = windows[0].ModelId;

        int rowY = listTop + 6;
        for (int i = 0; i < windows.Count && i < 4; i++)
        {
            FenceWindow win = windows[i];
            bool selected = win.ModelId == _selectedFenceId.Value;

            var rowRect = new Win32.RECT { left = ContentLeft + 6, top = rowY, right = ContentRight(clientRight) - 6, bottom = rowY + 28 };
            IntPtr rowBrush = Win32.CreateSolidBrush(selected
                ? (dark ? Win32.RGB(30, 60, 100) : Win32.RGB(230, 240, 250))
                : ClrSurface(dark));
            Win32.FillRect(hdc, ref rowRect, rowBrush);
            Win32.DeleteObject(rowBrush);

            var txtRect = new Win32.RECT { left = RowLabelLeft, top = rowY, right = ContentRight(clientRight) - 10, bottom = rowY + 28 };
            Win32.SetBkMode(hdc, Win32.TRANSPARENT);
            Win32.SetTextColor(hdc, selected ? ClrAccent(dark) : ClrText(dark));
            Win32.DrawText(hdc, win.Title, -1, ref txtRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

            Guid id = win.ModelId;
            AddHitTarget(rowRect, () => _selectedFenceId = id);
            rowY += 31;
        }

        FenceWindow? selectedFence = GetSelectedFence();
        if (selectedFence == null)
            return;

        int detailsY = listTop + listHeight + 8;
        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected fence sort", selectedFence.GetSortMode().ToString(), () =>
        {
            string next = NextString(selectedFence.GetSortMode().ToString(), SortModes);
            if (Enum.TryParse(next, ignoreCase: true, out FenceWindow.FenceSortMode mode))
                selectedFence.SortIcons(mode, animate: true);
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected fence icon size", $"{selectedFence.GetIconSize()}px", () =>
        {
            int next = NextInt(selectedFence.GetIconSize(), IconSizes);
            selectedFence.SetIconSize(next, notifyManager: true);
        }, dark, trackSettingChange: false);

        detailsY = DrawBoolRow(hdc, detailsY, clientRight, "Selected fence collapsed", selectedFence.IsCollapsed,
            () => selectedFence.ToggleCollapseExpand(), dark, trackSettingChange: false);

        detailsY = DrawBoolRow(hdc, detailsY, clientRight, "Selected fence locked", selectedFence.IsLocked(),
            selectedFence.ToggleLocked, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected titlebar mode", selectedFence.GetPerFenceTitlebarModeOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFenceTitlebarModeOverride(), PerFenceTitlebarModes);
            selectedFence.SetPerFenceTitlebarModeOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.titlebar.mode");
            selectedFence.InvalidateContent();
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected roll-up mode", selectedFence.GetPerFenceRollupModeOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFenceRollupModeOverride(), PerFenceRollupModes);
            selectedFence.SetPerFenceRollupModeOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.rollup.mode");
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected snap mode", selectedFence.GetPerFenceSnapModeOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFenceSnapModeOverride(), PerFenceSnapModes);
            selectedFence.SetPerFenceSnapModeOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.position.mode");
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected portal mode", selectedFence.GetPerFencePortalOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFencePortalOverride(), PerFencePortalModes);
            selectedFence.SetPerFencePortalOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.portal.mode");
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected glass panel", selectedFence.GetPerFenceBlurOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFenceBlurOverride(), PerFenceBlurModes);
            selectedFence.SetPerFenceBlurOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.blur.enabled");
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected live folder view", selectedFence.GetPerFencePortalViewOverride(), () =>
        {
            string next = NextString(selectedFence.GetPerFencePortalViewOverride(), LiveFolderViews);
            selectedFence.SetPerFencePortalViewOverride(next);
            SettingsManager.Instance.NotifyFenceChanged(selectedFence.ModelId, "fence.portal.view");
            selectedFence.InvalidateContent();
        }, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected fence color", selectedFence.GetFenceColorPresetLabel(),
            selectedFence.CycleFenceColorOverride, dark, trackSettingChange: false);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Selected bar mode", _manager.GetBarModeSummary(selectedFence.ModelId), () =>
        {
            bool changed = _manager.ToggleBarMode(selectedFence.ModelId);
            _statusText = changed
                ? $"Bar mode: {_manager.GetBarModeSummary(selectedFence.ModelId)}"
                : "Unable to toggle bar mode";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark, trackSettingChange: false);

        detailsY = DrawActionRow(hdc, detailsY, clientRight, "Bar mode", "Cycle dock edge", () =>
        {
            bool changed = _manager.CycleBarDockEdge(selectedFence.ModelId);
            _statusText = changed
                ? $"Bar mode: {_manager.GetBarModeSummary(selectedFence.ModelId)}"
                : "Enable bar mode first";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        detailsY = DrawActionRow(hdc, detailsY, clientRight, "Bar mode", "Cycle thickness", () =>
        {
            bool changed = _manager.CycleBarThickness(selectedFence.ModelId);
            _statusText = changed
                ? $"Bar mode: {_manager.GetBarModeSummary(selectedFence.ModelId)}"
                : "Enable bar mode first";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        detailsY = DrawChoiceRow(hdc, detailsY, clientRight, "Tab group", _manager.GetTabGroupSummary(selectedFence.ModelId), () =>
        {
            List<FenceWindow> windows = _manager.Windows.Where(w => w.ModelId != selectedFence.ModelId).ToList();
            if (windows.Count == 0)
            {
                _statusText = "Need at least two fences to create tabs";
                return;
            }

            FenceWindow target = windows[0];
            bool merged = _manager.MergeFencesIntoTabGroup(selectedFence.ModelId, target.ModelId);
            _statusText = merged
                ? $"Merged {selectedFence.Title} + {target.Title} into tab group"
                : "Unable to create tab group";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark, trackSettingChange: false);

        detailsY = DrawActionRow(hdc, detailsY, clientRight, "Tab group", "Next tab", () =>
        {
            bool switched = _manager.SwitchTabForFence(selectedFence.ModelId, +1);
            _statusText = switched ? "Switched to next tab" : "Fence is not in a tab group";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        detailsY = DrawActionRow(hdc, detailsY, clientRight, "Tab group", "Previous tab", () =>
        {
            bool switched = _manager.SwitchTabForFence(selectedFence.ModelId, -1);
            _statusText = switched ? "Switched to previous tab" : "Fence is not in a tab group";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        detailsY = DrawActionRow(hdc, detailsY, clientRight, "Tab group", "Dissolve tab group", () =>
        {
            bool dissolved = _manager.DissolveTabGroupForFence(selectedFence.ModelId);
            _statusText = dissolved ? "Tab group dissolved" : "Fence is not in a tab group";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        _ = DrawActionRow(hdc, detailsY, clientRight, "Selected fence name", "Rename", () =>
        {
            string? input = Win32InputDialog.Show(_hwnd, "Enter a new name for the selected fence.", "Rename Fence", selectedFence.Title);
            if (!string.IsNullOrWhiteSpace(input))
                selectedFence.Rename(input);
        }, dark);
    }

    private void DrawAdvancedTab(IntPtr hdc, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;
        DrawSectionHeader(hdc, "Visual Effects", y - 26, clientRight, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Enable blur background", _blurBackground, () => UpdateAndPreview(() => _blurBackground = !_blurBackground), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Glass strength", $"{_glassStrength}%", () => UpdateAndPreview(() =>
            _glassStrength = NextInt(_glassStrength, GlassStrengthOptions)), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Enable animations", _enableAnimations, () => UpdateAndPreview(() => _enableAnimations = !_enableAnimations), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Theme mode", _themeMode, () => UpdateAndPreview(() => _themeMode = NextString(_themeMode, ThemeOptions)), dark);

        DrawSectionHeader(hdc, "Portal Defaults", y + 8, clientRight, dark);
        y += 30;
        y = DrawBoolRow(hdc, y, clientRight, "Enable folder portals", _enableFolderPortalNavigation,
            () => UpdateAndPreview(() => _enableFolderPortalNavigation = !_enableFolderPortalNavigation), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Show folder icon in title bar", _liveFolderShowIcon,
            () => UpdateAndPreview(() => _liveFolderShowIcon = !_liveFolderShowIcon), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Default live folder view", _liveFolderDefaultView,
            () => UpdateAndPreview(() => _liveFolderDefaultView = NextString(_liveFolderDefaultView, LiveFolderViews)), dark);

        DrawSectionHeader(hdc, "Snapshots", y + 8, clientRight, dark);
        y += 30;

        y = DrawActionRow(hdc, y, clientRight, "Layout backup", "Create snapshot", () =>
        {
            string name = $"Settings backup {DateTime.Now:yyyy-MM-dd HH:mm}";
            SnapshotRepository.Instance.CreateSnapshot(name, "Created from settings window");
            _statusText = "Snapshot created";
        }, dark);

        _ = DrawActionRow(hdc, y, clientRight, "Layout restore", "Restore latest snapshot", RestoreLatestSnapshot, dark);
    }

    private void RestoreLatestSnapshot()
    {
        DetachedTaskObserver.Run(
            RestoreLatestSnapshotAsync(),
            ex => Log.Error(ex, "SettingsWindow: detached operation failed ({Operation})", "restore latest snapshot"));
    }

    private async Task RestoreLatestSnapshotAsync()
    {
        try
        {
            var snapshots = SnapshotRepository.Instance.GetAllSnapshots(includeAutoBackups: false);
            if (snapshots.Count == 0)
            {
                _statusText = "No snapshots available";
                Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
                return;
            }

            var latest = snapshots[0];
            bool restored = await SnapshotRepository.Instance.RestoreSnapshot(latest.Id);
            if (!restored)
            {
                _statusText = "Failed to restore snapshot";
                Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
                return;
            }

            await _manager.ReloadFromStateAsync();
            _statusText = $"Restored snapshot: {latest.Name}";
            Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "SettingsWindow: failed to restore latest snapshot");
            _statusText = "Failed to restore snapshot";
            Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
        }
    }

    private void DrawDesktopPagesTab(IntPtr hdc, int clientRight, bool dark)
    {
        RefreshDesktopPageState();

        int y = ScrolledContentTop;
        y = DrawBoolRow(hdc, y, clientRight, "Enable desktop pages", _enableDesktopPages,
            () => _enableDesktopPages = !_enableDesktopPages, dark);

        y = DrawChoiceRow(hdc, y, clientRight, "Active page", GetDesktopPageLabel(), () =>
        {
            int totalPages = Math.Max(1, PageService.Instance.TotalPages);
            int nextPage = (_currentDesktopPage + 1) % totalPages;
            PageService.Instance.GoToPage(nextPage);
            RefreshDesktopPageState();
            _statusText = $"Desktop page switched to {nextPage + 1}";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark, trackSettingChange: false);

        y = DrawActionRow(hdc, y, clientRight, "Navigation", "Previous page", () =>
        {
            PageService.Instance.PreviousPage();
            RefreshDesktopPageState();
            _statusText = $"Desktop page switched to {_currentDesktopPage + 1}";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        y = DrawActionRow(hdc, y, clientRight, "Navigation", "Next page", () =>
        {
            PageService.Instance.NextPage();
            RefreshDesktopPageState();
            _statusText = $"Desktop page switched to {_currentDesktopPage + 1}";
            SettingsManager.Instance.NotifyRuntimeRefresh();
        }, dark);

        y = DrawActionRow(hdc, y, clientRight, "Page management", "Create page", () =>
        {
            int pageIndex = PageService.Instance.CreateNewPage();
            RefreshDesktopPageState();
            _statusText = pageIndex >= 0
                ? $"Created desktop page {pageIndex + 1}"
                : "Failed to create page";
        }, dark);

        FenceWindow? selectedFence = GetSelectedFence();
        if (selectedFence == null)
        {
            _ = DrawActionRow(hdc, y, clientRight, "Selected fence", "No fence selected", () => { }, dark);
            return;
        }

        y = DrawChoiceRow(hdc, y, clientRight, "Selected fence", selectedFence.Title, CycleSelectedFence, dark, trackSettingChange: false);
        y = DrawChoiceRow(hdc, y, clientRight, "Selected fence page", $"Page {selectedFence.GetPageIndex() + 1}", () =>
        {
            int totalPages = Math.Max(1, PageService.Instance.TotalPages);
            int targetPage = (selectedFence.GetPageIndex() + 1) % totalPages;
            if (PageService.Instance.MoveFenceToPage(selectedFence.ModelId, targetPage))
            {
                RefreshDesktopPageState();
                _statusText = $"Moved {selectedFence.Title} to page {targetPage + 1}";
                SettingsManager.Instance.NotifyRuntimeRefresh();
            }
            else
            {
                _statusText = $"Failed to move {selectedFence.Title}";
            }
        }, dark, trackSettingChange: false);

        _ = DrawActionRow(hdc, y, clientRight, "Selected fence", "Move to active page", () =>
        {
            if (PageService.Instance.MoveFenceToPage(selectedFence.ModelId, _currentDesktopPage))
            {
                _statusText = $"Moved {selectedFence.Title} to active page {_currentDesktopPage + 1}";
                SettingsManager.Instance.NotifyRuntimeRefresh();
            }
            else
            {
                _statusText = $"Failed to move {selectedFence.Title}";
            }
        }, dark);
    }

    private void DrawRulesTab(IntPtr hdc, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;
        DrawSectionHeader(hdc, "Auto Placement", y - 26, clientRight, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Enable auto-placement rules", _enableGlobalPlacementRules, () =>
            UpdateAndPreview(() => _enableGlobalPlacementRules = !_enableGlobalPlacementRules), dark);

        y = DrawBoolRow(hdc, y, clientRight, "Enable quick-hide rule mode", _enableQuickHideMode, () =>
            UpdateAndPreview(() => _enableQuickHideMode = !_enableQuickHideMode), dark);

        y = DrawActionRow(hdc, y, clientRight, "Rule engine", "Apply rules now", () =>
        {
            int moved = _manager.ApplyAutoPlacementRulesLive();
            _statusText = moved > 0
                ? $"Rules applied: moved {moved} item(s)"
                : "Rules applied: no matching items";
        }, dark);

        y = DrawActionRow(hdc, y, clientRight, "Quick preset", "Sort by file type", () =>
        {
            int moved = _manager.ApplyFileTypePresetRules();
            _statusText = $"File type preset applied ({moved} moved)";
        }, dark);

        y = DrawActionRow(hdc, y, clientRight, "Quick preset", "Sort alphabetically", () =>
        {
            int moved = _manager.ApplyAlphabeticalPresetRules();
            _statusText = $"Alphabetical preset applied ({moved} moved)";
        }, dark);

        DrawSectionHeader(hdc, "Snap & Spacing", y + 8, clientRight, dark);
        y += 30;
        y = DrawBoolRow(hdc, y, clientRight, "Snap to screen edges", _snapToScreenEdges, () => UpdateAndPreview(() => _snapToScreenEdges = !_snapToScreenEdges), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Snap to other fences", _snapToOtherFences, () => UpdateAndPreview(() => _snapToOtherFences = !_snapToOtherFences), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Snap to grid", _snapToGrid, () => UpdateAndPreview(() => _snapToGrid = !_snapToGrid), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Inter-fence spacing", $"{_interFenceSpacing}px", () => UpdateAndPreview(() =>
            _interFenceSpacing = NextInt(_interFenceSpacing, SpacingOptions)), dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Icon spacing", $"{_iconSpacing}px", () => UpdateAndPreview(() =>
            _iconSpacing = NextInt(_iconSpacing, IconSpacingOptions)), dark);

        DrawSectionHeader(hdc, "Drag & Drop", y + 8, clientRight, dark);
        y += 30;
        y = DrawChoiceRow(hdc, y, clientRight, "Standard fence drop behavior", _standardFenceDropMode,
            () => _standardFenceDropMode = NextString(_standardFenceDropMode, StandardFenceDropModes), dark);
        y = DrawBoolRow(hdc, y, clientRight, "Confirm external drops", _confirmExternalDrops,
            () => _confirmExternalDrops = !_confirmExternalDrops, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Apply rules after drop", _autoApplyRulesOnDrop,
            () => _autoApplyRulesOnDrop = !_autoApplyRulesOnDrop, dark);
        y = DrawBoolRow(hdc, y, clientRight, "Highlight drop targets", _highlightDropTargets,
            () => _highlightDropTargets = !_highlightDropTargets, dark);

        FenceWindow? selectedFence = GetSelectedFence();
        if (selectedFence == null)
        {
            _ = DrawActionRow(hdc, y, clientRight, "Fence-specific rules", "No fence selected", () => { }, dark);
            return;
        }

        y = DrawChoiceRow(hdc, y, clientRight, "Selected fence", selectedFence.Title, CycleSelectedFence, dark, trackSettingChange: false);

        y = DrawActionRow(hdc, y, clientRight, "Quick preset", "Clear desktop to selected fence", () =>
        {
            int moved = _manager.ApplyCatchAllPresetRule(selectedFence.ModelId);
            _statusText = $"Catch-all preset applied to {selectedFence.Title} ({moved} moved)";
        }, dark);

        y = DrawActionRow(hdc, y, clientRight,
            "Include rules",
            string.IsNullOrWhiteSpace(selectedFence.GetIncludeRulesText()) ? "Edit" : selectedFence.GetIncludeRulesText(),
            () =>
            {
                string? input = Win32InputDialog.Show(
                    _hwnd,
                    "Comma-separated include rules. Examples: .lnk, .txt, project, work",
                    "Fence Include Rules",
                    selectedFence.GetIncludeRulesText());
                if (input != null)
                {
                    selectedFence.SetIncludeRulesText(input);
                    int moved = _manager.ApplyAutoPlacementRulesLive();
                    _statusText = $"Include rules updated ({moved} moved)";
                }
            }, dark);

        _ = DrawActionRow(hdc, y, clientRight,
            "Exclude rules",
            string.IsNullOrWhiteSpace(selectedFence.GetExcludeRulesText()) ? "Edit" : selectedFence.GetExcludeRulesText(),
            () =>
            {
                string? input = Win32InputDialog.Show(
                    _hwnd,
                    "Comma-separated exclude rules. Excludes override includes.",
                    "Fence Exclude Rules",
                    selectedFence.GetExcludeRulesText());
                if (input != null)
                {
                    selectedFence.SetExcludeRulesText(input);
                    int moved = _manager.ApplyAutoPlacementRulesLive();
                    _statusText = $"Exclude rules updated ({moved} moved)";
                }
            }, dark);
    }

    private void DrawPluginsTab(IntPtr hdc, int clientRight, bool dark)
    {
        int y = ScrolledContentTop;

        PluginThemeSnapshot themeSnapshot = ThemeService.Instance.GetCurrentTheme();
        PluginUpdateState updateState = PluginUpdateService.Instance.GetGlobalState();
        IReadOnlyList<PluginMetadata> pluginMetadata = PluginManagerService.Instance.GetPlugins();
        PluginTrustPolicyService trustPolicy = PluginTrustPolicyService.Instance;
        trustPolicy.Reload();

        DrawSectionHeader(hdc, "Host Services", y - 26, clientRight, dark);
        y = DrawChoiceRow(hdc, y, clientRight, "Current theme", themeSnapshot.ThemeMode, () => { }, dark, trackSettingChange: false);
        y = DrawChoiceRow(hdc, y, clientRight, "Accent color", themeSnapshot.AccentColorHex, () => { }, dark, trackSettingChange: false);
        y = DrawChoiceRow(hdc, y, clientRight, "Update state", updateState.Kind.ToString(), () => { }, dark, trackSettingChange: false);
        y = DrawChoiceRow(hdc, y, clientRight, "Trust policy",
            $"manifest signed: {trustPolicy.EnforceSignedManifest}, package signed: {trustPolicy.EnforceSignedPackage}",
            () => { }, dark, trackSettingChange: false);
        y = DrawActionRow(hdc, y, clientRight, "Refresh plugin status", "Refresh", () =>
        {
            PluginUpdateService.Instance.BeginCheck("settings refresh");
            PluginManagerService.Instance.RefreshFromManifests(AppPaths.PluginsDir, new PluginManifestReader());
            PluginUpdateService.Instance.RefreshFromMetadata(PluginManagerService.Instance.GetPlugins());
            _statusText = "Plugin status refreshed";
            Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
        }, dark);
        y = DrawActionRow(hdc, y, clientRight, "Install plugin package", "Install", () =>
        {
            string? packagePath = Win32InputDialog.Show(
                _hwnd,
                "Enter full path to a plugin package (.zip)",
                "Install Plugin Package",
                string.Empty);
            if (string.IsNullOrWhiteSpace(packagePath))
                return;

            string? expectedChecksum = Win32InputDialog.Show(
                _hwnd,
                "Optional SHA256 checksum (leave blank to trust plugin.json package checksum)",
                "Package Checksum",
                string.Empty);

            PluginInstallResult result = PluginPackageService.Instance.InstallPackageFromArchive(packagePath, expectedChecksum);
            _statusText = result.Success
                ? $"Installed {result.PluginId} {result.Version}"
                : $"Install failed: {result.Message}";

            PluginManagerService.Instance.RefreshFromManifests(AppPaths.PluginsDir, new PluginManifestReader());
            PluginUpdateService.Instance.RefreshFromMetadata(PluginManagerService.Instance.GetPlugins());
            Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
        }, dark);
        y = DrawActionRow(hdc, y, clientRight, "Rollback plugin install", "Rollback", () =>
        {
            string? pluginId = Win32InputDialog.Show(
                _hwnd,
                "Enter plugin id to rollback to the latest backup snapshot",
                "Rollback Plugin Install",
                string.Empty);

            if (string.IsNullOrWhiteSpace(pluginId))
                return;

            PluginInstallResult rollback = PluginPackageService.Instance.RollbackLastInstall(pluginId);
            _statusText = rollback.Success
                ? $"Rollback complete: {pluginId}"
                : $"Rollback failed: {rollback.Message}";

            PluginManagerService.Instance.RefreshFromManifests(AppPaths.PluginsDir, new PluginManifestReader());
            PluginUpdateService.Instance.RefreshFromMetadata(PluginManagerService.Instance.GetPlugins());
            Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
        }, dark);

        y += 10;
        DrawSectionHeader(hdc, "Loaded Plugins", y - 26, clientRight, dark);
        if (pluginMetadata.Count == 0)
        {
            y = DrawChoiceRow(hdc, y, clientRight, "Plugins", "None detected", () => { }, dark, trackSettingChange: false);
        }
        else
        {
            foreach (PluginMetadata metadata in pluginMetadata)
            {
                string loaded = metadata.IsLoaded ? "Loaded" : "Not loaded";
                y = DrawChoiceRow(hdc, y, clientRight, metadata.Name, $"{metadata.Version} | {loaded}", () => { }, dark, trackSettingChange: false);
                y = DrawChoiceRow(hdc, y, clientRight, "Compatibility", metadata.Compatibility, () => { }, dark, trackSettingChange: false);

                string state = metadata.IsEnabled ? "Enabled" : "Disabled";
                y = DrawActionRow(hdc, y, clientRight, "State", state == "Enabled" ? "Disable" : "Enable", () =>
                {
                    PluginManagerService.Instance.SetPluginEnabled(metadata.Id, !metadata.IsEnabled);
                    _statusText = metadata.IsEnabled
                        ? $"Disabled plugin: {metadata.Name}"
                        : $"Enabled plugin: {metadata.Name}";
                    Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
                }, dark);

                string updateText = metadata.UpdateAvailable
                    ? $"Update available: {metadata.LatestVersion ?? "new"}"
                    : "Up to date";
                y = DrawChoiceRow(hdc, y, clientRight, "Updates", updateText, () => { }, dark, trackSettingChange: false);

                int settingsCount = _pluginSettingDefinitions.Count(def =>
                    string.Equals(def.PluginId, metadata.Id, StringComparison.OrdinalIgnoreCase));
                y = DrawActionRow(hdc, y, clientRight, "Settings / config", "Open", () =>
                {
                    _statusText = settingsCount > 0
                        ? $"{metadata.Name}: {settingsCount} setting entries available below"
                        : $"{metadata.Name}: no registered plugin settings";
                    _scrollOffsetY = 0;
                    Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
                }, dark);

                y += 8;
            }
        }

        y += 10;
        if (_pluginSettingDefinitions.Count == 0)
        {
            DrawSectionHeader(hdc, "Plugin Settings", y - 26, clientRight, dark);
            var emptyRect = new Win32.RECT { left = ContentLeft, top = y, right = ContentRight(clientRight), bottom = y + 40 };
            Win32.SetBkMode(hdc, Win32.TRANSPARENT);
            Win32.SetTextColor(hdc, ClrTextMuted(dark));
            Win32.DrawText(hdc, "No plugin settings have been registered.", -1, ref emptyRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
            return;
        }

        string? lastSection = null;
        for (int i = 0; i < _pluginSettingDefinitions.Count; i++)
        {
            PluginSettingDefinition definition = _pluginSettingDefinitions[i];
            SettingDefinition blueprint = _pluginSettingBlueprints[i];
            string sectionKey = $"{blueprint.Tab}:{blueprint.Section}";
            if (!string.Equals(lastSection, sectionKey, StringComparison.Ordinal))
            {
                DrawSectionHeader(hdc, $"{blueprint.Tab} / {blueprint.Section}", y - 26, clientRight, dark);
                lastSection = sectionKey;
            }

            y = DrawPluginSettingRow(hdc, y, clientRight, definition, blueprint, dark);
            y += 2;
        }
    }

    private int DrawPluginSettingRow(IntPtr hdc, int y, int clientRight, PluginSettingDefinition definition, SettingDefinition blueprint, bool dark)
    {
        string currentValue = GetPluginSettingValue(definition);
        return blueprint.Kind switch
        {
            SettingValueKind.Toggle => DrawBoolRow(hdc, y, clientRight, blueprint.Label, ParsePluginToggle(currentValue), () =>
                SetPluginSettingValue(definition, (!ParsePluginToggle(currentValue)).ToString().ToLowerInvariant()), dark, trackSettingChange: false),
            SettingValueKind.Choice => DrawChoiceRow(hdc, y, clientRight, blueprint.Label, currentValue, () =>
                SetPluginSettingValue(definition, NextString(currentValue, blueprint.Choices ?? Array.Empty<string>())), dark, trackSettingChange: false),
            SettingValueKind.Action => DrawActionRow(hdc, y, clientRight, blueprint.Label, "Trigger", () =>
            {
                SettingsEvents.Raise($"plugin.{definition.PluginId}.{definition.Key}", "plugin");
                _statusText = $"Triggered plugin action: {blueprint.Label}";
            }, dark),
            _ => DrawActionRow(hdc, y, clientRight, blueprint.Label, string.IsNullOrWhiteSpace(currentValue) ? "Edit" : currentValue, () =>
            {
                string prompt = blueprint.Kind == SettingValueKind.Color
                    ? "Enter a color value, for example #FFF9C4"
                    : $"Enter a value for {blueprint.Label}.";
                string? input = Win32InputDialog.Show(_hwnd, prompt, blueprint.Label, currentValue);
                if (input != null)
                {
                    SetPluginSettingValue(definition, input);
                    _statusText = $"Updated plugin setting: {blueprint.Label}";
                }
            }, dark),
        };
    }

    private string GetPluginSettingValue(PluginSettingDefinition definition)
    {
        return PluginSettingsStore.Instance.Get(definition.PluginId, definition.Key, definition.DefaultValue)
            ?? string.Empty;
    }

    private void SetPluginSettingValue(PluginSettingDefinition definition, string value)
    {
        PluginSettingsStore.Instance.Set(definition.PluginId, definition.Key, value);
        Win32.InvalidateRect(_hwnd, IntPtr.Zero, true);
    }

    private static bool ParsePluginToggle(string? value)
    {
        return bool.TryParse(value, out bool parsed) && parsed;
    }

    private void CycleSelectedFence()
    {
        List<FenceWindow> windows = _manager.Windows.ToList();
        if (windows.Count == 0)
            return;

        if (!_selectedFenceId.HasValue)
        {
            _selectedFenceId = windows[0].ModelId;
            return;
        }

        int currentIndex = windows.FindIndex(w => w.ModelId == _selectedFenceId.Value);
        if (currentIndex < 0)
        {
            _selectedFenceId = windows[0].ModelId;
            return;
        }

        int nextIndex = (currentIndex + 1) % windows.Count;
        _selectedFenceId = windows[nextIndex].ModelId;
    }

    private void RefreshDesktopPageState()
    {
        _desktopPageCount = Math.Max(1, PageService.Instance.TotalPages);
        _currentDesktopPage = Math.Clamp(PageService.Instance.CurrentPageIndex, 0, _desktopPageCount - 1);
    }

    private string GetDesktopPageLabel()
    {
        return $"Page {_currentDesktopPage + 1} of {_desktopPageCount}";
    }

    // Blueprint hook for future searchable/collapsible settings rendering.
    private IReadOnlyList<SettingsControlSkeleton> SearchSkeletonControls(string query)
    {
        return _skeleton.SearchControls(query);
    }
}
