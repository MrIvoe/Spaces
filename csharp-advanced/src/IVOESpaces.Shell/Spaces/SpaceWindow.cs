using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text;
using IVOESpaces.Core;
using IVOESpaces.Core.Models;
using IVOESpaces.Shell.Native;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Desktop;
using Serilog;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// A single space container as a real Win32 window (HWND) parented to WorkerW.
/// No WPF, no WinForms — pure user32 + gdi32.
///
/// Per-instance data (title, bounds) is stored on the instance and looked up
/// in the static WndProc via <see cref="_instances"/>.
/// </summary>
internal sealed class SpaceWindow
{
    public enum SpaceDomainMutationKind
    {
        LayoutChanged,
        ContentChanged,
        SettingsChanged,
        OwnershipChanged,
        UsageChanged,
    }

    public enum SpaceSortMode
    {
        Manual,
        Name,
        Type,
        DateModified,
        Usage,
    }

    private const string ClassName = AppIdentity.InternalName + "_SpaceWindow";

    // Context menu command IDs
    private const uint CmdSortManual = 2001;
    private const uint CmdSortName = 2002;
    private const uint CmdSortType = 2003;
    private const uint CmdSortDate = 2004;
    private const uint CmdSortUsage = 2005;
    private const uint CmdSize16 = 2010;
    private const uint CmdSize32 = 2011;
    private const uint CmdSize48 = 2012;
    private const uint CmdSize64 = 2013;
    private const uint CmdAdjustSpacing = 2014;
    private const uint CmdRename = 2021;
    private const uint CmdDelete = 2022;
    private const uint CmdToggleLock = 2023;
    private const uint WmDesktopSync = Win32.WM_USER + 50;

    // Static class registration + HWND→instance lookup
    private static bool _classRegistered;
    private static Win32.WndProc? _wndProcDelegate;
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<IntPtr, SpaceWindow> _instances = new();

    private const int MinWidth  = 150;
    private const int MinHeight = 100;

    public IntPtr Handle  { get; private set; } = IntPtr.Zero;
    public Guid   ModelId { get; }
    public string Title   => _model.Title;
    public int    X       { get; private set; }
    public int    Y       { get; private set; }
    public int    Width   { get; private set; }
    public int    Height  { get; private set; }

    public bool IsAlive => Handle != IntPtr.Zero && Win32.IsWindow(Handle);

    // Cached window rect for WM_NCHITTEST — updated on WM_WINDOWPOSCHANGED
    // to avoid calling GetWindowRect on every mouse-move message.
    private Win32.RECT _cachedWindowRect;

    // SpaceModel bound to this window; provides items, roll-up state, and
    // fractions that are written back on WM_EXITSIZEMOVE.
    private SpaceModel _model = null!;   // assigned in constructor
    private bool _rolledUp;
    private int  _preRollupHeight;

    // Pre-computed icon grid layout — invalidated on WM_SIZE, consumed by Paint.
    private readonly SpaceIconLayout _layout = new();

    // Tab and search management
    private readonly SpaceTabManager _tabManager;
    private readonly SpaceSearchFilter _searchFilter = new();
    private readonly IconHoverPreview _hoverPreview = new();
    private readonly SpaceAnimationEngine _animations = new();
    private readonly QuickActionManager _quickActions = new();
    private Guid? _hoveredItemId;
    private Guid? _selectedItemId;
    private int _opacityPercent;
    private bool _isDragging;
    private bool _isMouseInClient;
    private bool _trackingMouseLeave;
    private int _scrollOffset;
    private SpaceSortMode _currentSortMode;
    private int _iconSize;
    private string? _livePreviewTitlebarMode;
    private string? _livePreviewRollupMode;
    private bool? _livePreviewSnapToGrid;
    private bool? _livePreviewPortalEnabled;
    private bool? _livePreviewBlurEnabled;
    private int? _livePreviewGlassStrength;
    private int? _livePreviewIconSpacing;
    private SpaceOleDropTarget? _oleDropTarget;
    private bool _externalDropHover;

    private static DragPayload? _activeDrag;
    private static readonly string[] _desktopRoots = BuildDesktopRoots();

    private sealed record DragPayload(IntPtr SourceHwnd, Guid ItemId);

    public event Action<HoverPreviewData>? HoverPreviewChanged;
    public event Action? HoverPreviewHidden;
    public event Action<SpaceWindow>? DeleteRequested;
    public event Action<SpaceWindow, int>? IconSizeChanged;
    public event Action<int>? InterSpaceSpacingChangeRequested;
    public event Action<SpaceWindow, SpaceDomainMutationKind>? DomainMutationRequested;
    public static event Action? DesktopSyncRequested;

    private void ReportDomainMutation(SpaceDomainMutationKind kind)
    {
        DomainMutationRequested?.Invoke(this, kind);
    }

    // Animated roll-up state
    private static readonly IntPtr RollTimerId  = new(1);
    private const           int    RollSteps      = 8;
    private const           int    RollIntervalMs = 16;
    private bool _rolling;
    private int  _rollTargetHeight;
    private int  _rollStartHeight;
    private int  _rollStepIndex;

    // Snap-flash visual feedback (200 ms blue border on snap during drag)
    private static readonly IntPtr SnapFlashTimerId = new(2);
    private const           int    SnapFlashMs      = 200;
    private bool _snapFlashActive;

    // Cross-thread bounds queue — AnimationManager posts SetWindowPos via PostMessage
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<int, (int x, int y, int w, int h)> _pendingBounds = new();
    private static int _pendingBoundsKey;

    public SpaceWindow(SpaceModel model, int x, int y, int width, int height)
    {
        _model  = model;
        ModelId = model.Id;
        X       = x;
        Y       = y;
        Width   = width;
        Height  = height;
        _rolledUp        = model.IsRolledUp;
        _preRollupHeight = height;
        _tabManager      = new SpaceTabManager(model);
        _opacityPercent  = Math.Clamp(AppSettingsRepository.Instance.Current.SpaceOpacity, 20, 100);
        _iconSize        = Math.Clamp(model.IconSizeOverride ?? AppSettingsRepository.Instance.Current.IconSize, 16, 96);
        _currentSortMode = ParseSortMode(model.SortMode);

        _quickActions.AttachActions(this);
    }

    // ── Lifecycle ───────────────────────────────────────────────────────────

    /// <summary>Create the Win32 window and parent it to the WorkerW desktop layer.</summary>
    public void Create(bool initiallyVisible = true)
    {
        EnsureClassRegistered();

        IntPtr workerW = DesktopHost.Instance.WorkerW;
        if (workerW == IntPtr.Zero)
        {
            Log.Warning("SpaceWindow '{Title}': WorkerW not available — cannot create window (will retry via cooperative delays)", Title);
            return;
        }

        // Create hidden first so early WM_PAINT/WM_SIZE do not race before registration.
        Handle = Win32.CreateWindowEx(
            Win32.WS_EX_TOOLWINDOW | Win32.WS_EX_NOACTIVATE | (int)Win32.WS_EX_LAYERED,
            ClassName,
            Title,
            Win32.WS_POPUP,
            X, Y, Width, Height,
            workerW,
            IntPtr.Zero,
            Win32.GetModuleHandle(null),
            IntPtr.Zero);

        if (Handle == IntPtr.Zero)
        {
            Log.Warning("SpaceWindow '{Title}': CreateWindowEx failed (err {Err}) — parent WorkerW was {WW:X}",
                Title, Marshal.GetLastWin32Error(), workerW);
            return;
        }

        // Register HWND → this instance for the static WndProc
        _instances[Handle] = this;

        // Accept file drops from Explorer/desktop (WM_DROPFILES).
        Shell32.DragAcceptFiles(Handle, true);

        // Register OLE drop target for richer Explorer drag/drop payloads.
        _oleDropTarget = new SpaceOleDropTarget(this);
        int registerResult = Shell32.RegisterDragDrop(Handle, _oleDropTarget);
        if (registerResult != 0)
            Log.Warning("SpaceWindow '{Title}': RegisterDragDrop failed (hr=0x{Hr:X8})", Title, registerResult);

        // Opacity from settings (default 85 %)
        SetOpacityPercent(_opacityPercent);

        // Optional acrylic blur (controlled by BlurBackground setting)
        RefreshBlurBackground();

        _layout.Invalidate();
        UpdateScrollInfo(GetDisplayedItems().Count);

        Win32.SetWindowPos(
            Handle,
            Win32.HWND_BOTTOM,
            X, Y, Width, Height,
            Win32.SWP_NOACTIVATE);

        Win32.ShowWindow(Handle, initiallyVisible ? Win32.SW_SHOWNOACTIVATE : Win32.SW_HIDE);
        Win32.InvalidateRect(Handle, IntPtr.Zero, false);
        Win32.UpdateWindow(Handle);

        Log.Information("SpaceWindow '{Title}': created HWND={H:X} at ({X},{Y}) {W}x{H} visible={Visible}",
            Title, Handle, X, Y, Width, Height, initiallyVisible);
    }

    /// <summary>Re-parent this window to the new WorkerW after Explorer restart.</summary>
    public void ReAnchorToDesktop(IntPtr newWorkerW)
    {
        if (!IsAlive) return;
        bool wasVisible = Win32.IsWindowVisible(Handle);
        Win32.SetParent(Handle, newWorkerW);
        Win32.SetWindowPos(Handle, Win32.HWND_BOTTOM, X, Y, Width, Height,
            Win32.SWP_NOACTIVATE);
        Win32.ShowWindow(Handle, wasVisible ? Win32.SW_SHOWNOACTIVATE : Win32.SW_HIDE);
        Log.Debug("SpaceWindow '{Title}': re-anchored to WorkerW={WW:X} visible={Visible}", Title, newWorkerW, wasVisible);
    }

    public void SetBounds(int x, int y, int width, int height)
    {
        X = x; Y = y; Width = width; Height = height;
        if (IsAlive)
            Win32.SetWindowPos(Handle, Win32.HWND_BOTTOM, x, y, width, height,
                Win32.SWP_NOACTIVATE);
    }

    public void SetVisible(bool visible)
    {
        if (!IsAlive) return;
        if (visible)
        {
            Win32.ShowWindow(Handle, Win32.SW_SHOWNOACTIVATE);
            Win32.SetWindowPos(Handle, Win32.HWND_BOTTOM, X, Y, Width, Height,
                Win32.SWP_NOACTIVATE | Win32.SWP_NOMOVE | Win32.SWP_NOSIZE | Win32.SWP_SHOWWINDOW);
            return;
        }

        Win32.ShowWindow(Handle, Win32.SW_HIDE);
    }

    public void SetTopmost(bool topmost)
    {
        if (!IsAlive) return;
        Win32.SetWindowPos(
            Handle,
            topmost ? Win32.HWND_TOPMOST : Win32.HWND_BOTTOM,
            X,
            Y,
            Width,
            Height,
            Win32.SWP_NOACTIVATE | Win32.SWP_SHOWWINDOW);
    }

    public int GetPageIndex()
    {
        return _model.PageIndex;
    }

    public bool IsMarkedHidden() => _model.IsHidden;

    public void SetBlurBackground(bool enabled)
    {
        SetBlurBackground(enabled, AppSettingsRepository.Instance.Current.GlassStrength);
    }

    public void SetBlurBackground(bool enabled, int strengthPercent)
    {
        if (!IsAlive) return;
        bool darkMode = ThemeEngine.Instance.IsDarkMode;
        if (enabled)
            DwmApi.ApplyAcrylic(Handle, darkMode, strengthPercent);
        else
            DwmApi.RemoveAcrylic(Handle);
    }

    public void RefreshBlurBackground()
    {
        bool enabled = GetEffectiveBlurEnabled();
        int strength = GetEffectiveGlassStrength();
        SetBlurBackground(enabled, strength);
    }

    /// <summary>
    /// Thread-safe variant: posts <see cref="Win32.WM_APP_SETBOUNDS"/> so that the
    /// actual <c>SetWindowPos</c> call is executed on the STA message-loop thread.
    /// Use this from thread-pool code (e.g. AnimationManager async tasks).
    /// </summary>
    public void PostSetBounds(int x, int y, int width, int height)
    {
        X = x; Y = y; Width = width; Height = height;
        if (!IsAlive) return;
        int key = System.Threading.Interlocked.Increment(ref _pendingBoundsKey);
        _pendingBounds[key] = (x, y, width, height);
        Win32.PostMessage(Handle, Win32.WM_APP_SETBOUNDS, new IntPtr(key), IntPtr.Zero);
    }

    public void Destroy()
    {
        if (Handle != IntPtr.Zero)
        {
            Shell32.DragAcceptFiles(Handle, false);
            int revokeResult = Shell32.RevokeDragDrop(Handle);
            if (revokeResult != 0)
                Log.Debug("SpaceWindow '{Title}': RevokeDragDrop returned hr=0x{Hr:X8}", Title, revokeResult);
        }

        _oleDropTarget = null;
        if (IsAlive)
            Win32.DestroyWindow(Handle);
        Handle = IntPtr.Zero;
    }

    // ── Window procedure ───────────────────────────────────────────────────

    private static IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        _instances.TryGetValue(hwnd, out SpaceWindow? fw);

        switch (msg)
        {
            case Win32.WM_PAINT:
                if (fw != null)
                {
                    int titleBarHeight = fw.GetTitleBarHeight();
                    int iconSpacing = fw.GetEffectiveIconSpacing();
                    var displayedItems = fw.GetDisplayedItems();
                    var positions = fw._layout.GetPositions(displayedItems, fw.Width, fw.GetIconSize(), iconSpacing, titleBarHeight);
                    fw.UpdateScrollInfo(displayedItems.Count);
                    bool showTitleBar = fw.ShouldRenderTitlebar();
                    string portalViewMode = fw.GetEffectivePortalViewMode();
                    SpaceRenderer.Paint(hwnd, fw.Title,
                        displayedItems, fw._rolledUp, positions, fw.GetIconSize(), fw._searchFilter.Query, fw._tabManager, fw._model.IsAiSuggested,
                        showTitleBar, fw._model.BackgroundColorOverride, fw._model.TitleColorOverride, fw._scrollOffset, fw._snapFlashActive || fw._externalDropHover,
                        fw.IsLocked(),
                        fw._selectedItemId,
                        fw._hoveredItemId,
                        fw._model.Type == SpaceType.Portal, portalViewMode, iconSpacing);
                }
                else
                    SpaceRenderer.Paint(hwnd, "Space");
                return IntPtr.Zero;

            case Win32.WM_ERASEBKGND:
                return new IntPtr(1); // prevent background flicker

            // WM_SIZE fires on every resize tick during a drag-resize.
            // Invalidate the cached icon layout so positions are recalculated
            // for the new width on the next WM_PAINT.
            case Win32.WM_SIZE:
                if (fw != null)
                {
                    fw._layout.Invalidate();
                    fw.UpdateScrollInfo(fw.GetDisplayedItems().Count);
                }
                break;

            case WmDesktopSync:
                DesktopSyncRequested?.Invoke();
                return IntPtr.Zero;

            case Win32.WM_NCLBUTTONDBLCLK:
                // Double-click on title bar — begin animated roll-up / expand
                if (fw != null && wParam.ToInt32() == Win32.HTCAPTION)
                {
                    string rollupMode = fw.GetEffectiveRollupMode();
                    if (!string.Equals(rollupMode, "Disabled", StringComparison.OrdinalIgnoreCase))
                        fw.ToggleRollUp();
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_TIMER:
                // Roll-up animation tick
                if (fw != null && wParam == RollTimerId)
                {
                    fw.OnRollTick();
                    return IntPtr.Zero;
                }
                // Snap-flash timer: clear the blue border after 200 ms
                if (fw != null && wParam == SnapFlashTimerId)
                {
                    fw._snapFlashActive = false;
                    Win32.KillTimer(hwnd, SnapFlashTimerId);
                    Win32.InvalidateRect(hwnd, IntPtr.Zero, false);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_MOUSEMOVE:
                if (fw != null)
                {
                    fw.OnMouseMoveTracked();
                    int lx = Win32.GET_X_LPARAM(lParam);
                    int ly = Win32.GET_Y_LPARAM(lParam);
                    fw.HandleDragMove(lx, ly);
                    fw.HandleHoverMove(lx, ly);
                }
                break;

            case Win32.WM_MOUSEWHEEL:
                if (fw != null)
                {
                    int delta = Win32.HIWORD(wParam);
                    fw.ScrollBy(-(delta / 120) * Math.Max(24, fw.GetIconSize()));
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_VSCROLL:
                if (fw != null)
                {
                    fw.HandleVScroll(wParam);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_MOUSELEAVE:
                if (fw != null)
                {
                    fw.OnMouseLeaveTracked();
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_LBUTTONDOWN:
                if (fw != null)
                {
                    int lx = Win32.GET_X_LPARAM(lParam);
                    int ly = Win32.GET_Y_LPARAM(lParam);
                    fw.HandleLeftButtonDown(lx, ly);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_LBUTTONUP:
                if (fw != null)
                {
                    int lx = Win32.GET_X_LPARAM(lParam);
                    int ly = Win32.GET_Y_LPARAM(lParam);
                    fw.HandleLeftButtonUp(lx, ly);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_LBUTTONDBLCLK:
                if (fw != null)
                {
                    int lx = Win32.GET_X_LPARAM(lParam);
                    int ly = Win32.GET_Y_LPARAM(lParam);
                    fw.HandleItemDoubleClick(lx, ly);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_RBUTTONUP:
                if (fw != null)
                {
                    int lx = Win32.GET_X_LPARAM(lParam);
                    int ly = Win32.GET_Y_LPARAM(lParam);
                    fw.HandleRightClick(lx, ly);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_DROPFILES:
                if (fw != null)
                {
                    fw.HandleShellDrop(wParam);
                    return IntPtr.Zero;
                }
                break;

            // WM_WINDOWPOSCHANGED fires after every move or resize — much less
            // often than WM_NCHITTEST — so we refresh the rect cache here.
            case Win32.WM_WINDOWPOSCHANGED:
                if (fw != null && Win32.GetWindowRect(hwnd, out Win32.RECT updatedRect))
                    fw._cachedWindowRect = updatedRect;
                break; // fall through to DefWindowProc

            // Invalidate the cached work area so the next conversion uses the
            // correct dimensions after a resolution or DPI change.
            case Win32.WM_DISPLAYCHANGE:
                DesktopHost.Instance.InvalidateWorkArea();
                break;

            case Win32.WM_DPICHANGED:
                if (fw != null)
                {
                    unsafe
                    {
                        var suggested = (Win32.RECT*)lParam;
                        int width = Math.Max(suggested->right - suggested->left, MinWidth);
                        int height = Math.Max(suggested->bottom - suggested->top, MinHeight);
                        fw.SetBounds(suggested->left, suggested->top, width, height);
                    }

                    fw._layout.Invalidate();
                    fw.UpdateScrollInfo(fw.GetDisplayedItems().Count);
                    Win32.InvalidateRect(hwnd, IntPtr.Zero, false);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_NCHITTEST:
            {
                // Use cached rect — avoids a kernel round-trip on every mouse move.
                Win32.RECT wr = fw?._cachedWindowRect ?? default;
                if (wr.right == 0) // not yet populated (first hit before first WINDOWPOSCHANGED)
                    Win32.GetWindowRect(hwnd, out wr);

                int mx = Win32.GET_X_LPARAM(lParam);
                int my = Win32.GET_Y_LPARAM(lParam);
                const int B = 6; // resize border thickness (pixels)

                if (fw != null && (fw.IsLocked() || fw.IsBarMode()))
                    return (IntPtr)Win32.HTCLIENT;

                bool onLeft   = mx < wr.left   + B;
                bool onRight  = mx > wr.right  - B;
                bool onTop    = my < wr.top    + B;
                bool onBottom = my > wr.bottom - B;

                if (onLeft  && onTop)    return (IntPtr)Win32.HTTOPLEFT;
                if (onRight && onTop)    return (IntPtr)Win32.HTTOPRIGHT;
                if (onLeft  && onBottom) return (IntPtr)Win32.HTBOTTOMLEFT;
                if (onRight && onBottom) return (IntPtr)Win32.HTBOTTOMRIGHT;
                if (onTop)               return (IntPtr)Win32.HTTOP;
                if (onBottom)            return (IntPtr)Win32.HTBOTTOM;
                if (onLeft)              return (IntPtr)Win32.HTLEFT;
                if (onRight)             return (IntPtr)Win32.HTRIGHT;

                // Title bar area — enables OS drag-to-move
                if (my < wr.top + (fw?.GetTitleBarHeight() ?? SpaceRenderer.TitleBarHeight))
                    return (IntPtr)Win32.HTCAPTION;

                return (IntPtr)Win32.HTCLIENT;
            }

            case Win32.WM_GETMINMAXINFO:
            {
                // Enforce minimum track size so the space can't be dragged smaller
                unsafe
                {
                    var mmi = (Win32.MINMAXINFO*)lParam;
                    mmi->ptMinTrackSize.x = MinWidth;
                    mmi->ptMinTrackSize.y = MinHeight;
                }
                return IntPtr.Zero;
            }

            // Live snap during move drag — OS sends this for every mouse-move pixel.
            // We adjust the RECT* in lParam in place and return TRUE.
            case Win32.WM_MOVING:
            {
                if (fw != null)
                {
                    unsafe
                    {
                        var proposed = (Win32.RECT*)lParam;
                        Win32.RECT wa = GetMonitorWorkArea(hwnd);
                        var settings = AppSettingsRepository.Instance.Current;
                        bool snapped = LiveSnapRect(proposed, wa, hwnd, fw, settings, isResize: false);
                        if (snapped && !fw._snapFlashActive)
                        {
                            fw._snapFlashActive = true;
                            Win32.InvalidateRect(hwnd, IntPtr.Zero, false);
                            Win32.SetTimer(hwnd, SnapFlashTimerId, SnapFlashMs, IntPtr.Zero);
                        }
                    }
                }
                return new IntPtr(1); // TRUE — rect was modified
            }

            // Live snap during resize drag.
            case Win32.WM_SIZING:
            {
                if (fw != null)
                {
                    unsafe
                    {
                        var proposed = (Win32.RECT*)lParam;
                        Win32.RECT wa = GetMonitorWorkArea(hwnd);
                        var settings = AppSettingsRepository.Instance.Current;
                        bool snapped = LiveSnapRect(proposed, wa, hwnd, fw, settings, isResize: true);
                        if (snapped && !fw._snapFlashActive)
                        {
                            fw._snapFlashActive = true;
                            Win32.InvalidateRect(hwnd, IntPtr.Zero, false);
                            Win32.SetTimer(hwnd, SnapFlashTimerId, SnapFlashMs, IntPtr.Zero);
                        }
                    }
                }
                return new IntPtr(1); // TRUE — rect was modified
            }

            // Cross-thread SetWindowPos: AnimationManager posts this from thread pool.
            case Win32.WM_APP_SETBOUNDS:
            {
                int key = wParam.ToInt32();
                if (_pendingBounds.TryRemove(key, out var b))
                    Win32.SetWindowPos(hwnd, Win32.HWND_BOTTOM, b.x, b.y, b.w, b.h, Win32.SWP_NOACTIVATE);
                return IntPtr.Zero;
            }

            case Win32.WM_EXITSIZEMOVE:
            {
                // Update cached bounds after a drag or resize completes, then persist
                if (fw != null && Win32.GetWindowRect(hwnd, out Win32.RECT wr))
                {
                    fw.X      = wr.left;
                    fw.Y      = wr.top;
                    fw.Width  = wr.right  - wr.left;
                    fw.Height = wr.bottom - wr.top;
                    fw._layout.Invalidate(); // width may have changed during resize
                    Log.Debug("SpaceWindow '{T}': moved/resized to ({X},{Y}) {W}x{H}",
                        fw.Title, fw.X, fw.Y, fw.Width, fw.Height);

                    // Use the monitor this window currently lives on for fractions
                    // so positions are DPI-independent on every monitor.
                    Win32.RECT workArea = GetMonitorWorkArea(hwnd);

                    // Snap to screen edges if the setting is enabled
                    var appSettings = AppSettingsRepository.Instance.Current;
                    if (fw.ShouldSnapToEdges(appSettings))
                        SnapToEdge(fw, workArea, appSettings.SnapThreshold);

                    if (fw.ShouldSnapToGrid(appSettings))
                        SnapToGrid(fw, workArea, Math.Max(4, appSettings.GridSize));

                    // Persist per-monitor fractions
                    int waW = Math.Max(workArea.right  - workArea.left, 1);
                    int waH = Math.Max(workArea.bottom - workArea.top,  1);
                    fw._model.XFraction      = (double)(fw.X - workArea.left) / waW;
                    fw._model.YFraction      = (double)(fw.Y - workArea.top)  / waH;
                    fw._model.WidthFraction  = (double)fw.Width  / waW;
                    fw._model.HeightFraction = (double)fw.Height / waH;

                    // Track monitor identity so startup can restore on the correct display.
                    IntPtr hMon = Win32.MonitorFromWindow(hwnd, Win32.MONITOR_DEFAULTTONEAREST);
                    var mi = new Win32.MONITORINFOEX { cbSize = (uint)Marshal.SizeOf<Win32.MONITORINFOEX>() };
                    if (hMon != IntPtr.Zero && Win32.GetMonitorInfo(hMon, ref mi))
                        fw._model.MonitorDeviceName = mi.szDevice;

                    fw.ReportDomainMutation(SpaceDomainMutationKind.LayoutChanged);
                }
                return IntPtr.Zero;
            }

            case Win32.WM_DESTROY:
                if (fw != null)
                {
                    Win32.KillTimer(hwnd, RollTimerId);      // safety cleanup
                    Win32.KillTimer(hwnd, SnapFlashTimerId); // safety cleanup
                }
                _instances.TryRemove(hwnd, out _);
                return IntPtr.Zero;
        }

        return Win32.DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // ── Roll-up ───────────────────────────────────────────────────────────

    /// <summary>
    /// Start the collapse / expand animation.  The actual height change is
    /// applied one step at a time in <see cref="OnRollTick"/> (WM_TIMER).
    /// </summary>
    private void ToggleRollUp()
    {
        if (_rolling) return; // ignore double-click during animation

        _rollStartHeight = Height;
        _rollStepIndex = 0;

        if (!_rolledUp)
        {
            _preRollupHeight = Height;
            _rollTargetHeight = GetTitleBarHeight();
        }
        else
        {
            int titleBarHeight = GetTitleBarHeight();
            _rollTargetHeight = _preRollupHeight > titleBarHeight
                ? _preRollupHeight
                : titleBarHeight * 4;
        }

        _rolling = true;
        Win32.SetTimer(Handle, RollTimerId, RollIntervalMs, IntPtr.Zero);
    }

    /// <summary>WM_TIMER callback: advance one animation step toward the target height.</summary>
    private void OnRollTick()
    {
        _rollStepIndex = Math.Min(_rollStepIndex + 1, RollSteps);
        double t = (double)_rollStepIndex / RollSteps;
        int newH = (int)Math.Round(_rollStartHeight + ((_rollTargetHeight - _rollStartHeight) * t));
        int titleBarHeight = GetTitleBarHeight();
        newH = Math.Clamp(newH,
                  titleBarHeight,
                          Math.Max(_preRollupHeight, _rollTargetHeight));

        Height = newH;
        Win32.SetWindowPos(Handle, Win32.HWND_BOTTOM, X, Y, Width, Height,
                           Win32.SWP_NOACTIVATE | Win32.SWP_NOMOVE);
        Win32.InvalidateRect(Handle, IntPtr.Zero, false);

        if (_rollStepIndex >= RollSteps || Height == _rollTargetHeight)
        {
            _rolling  = false;
            _rolledUp = !_rolledUp;
            Win32.KillTimer(Handle, RollTimerId);
            _model.IsRolledUp = _rolledUp;
            ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);

            if (_rolledUp)
                _animations.OnSpaceCollapsed(ModelId);
            else
                _animations.OnSpaceExpanded(ModelId);
        }
    }

    public bool IsCollapsed => _rolledUp;

    public void ToggleCollapseExpand()
    {
        ToggleRollUp();
    }

    public bool IsLocked() => _model.IsLocked;

    public bool IsBarMode() => _model.IsBar && _model.DockEdge != DockEdge.None;

    public Guid? GetTabContainerId() => _model.TabContainerId;

    public int GetTabIndex() => _model.TabIndex;

    public void SetLocked(bool locked)
    {
        if (_model.IsLocked == locked)
            return;

        _model.IsLocked = locked;
        InvalidateContent();
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public void ToggleLocked()
    {
        SetLocked(!_model.IsLocked);
    }

    public void Rename(string newTitle)
    {
        if (string.IsNullOrWhiteSpace(newTitle))
            return;

        _model.Title = newTitle.Trim();
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);

        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public void MarkAsAISuggested()
    {
        _model.IsAiSuggested = true;
        _model.AiSuggestedAtUtc = DateTime.UtcNow;
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public void SortIconsByUsage(bool animate = false)
    {
        SortIcons(SpaceSortMode.Usage, animate);
    }

    public void SortIcons(SpaceSortMode mode, bool animate = false)
    {
        var before = _model.Items.Select(i => i.Id).ToList();
        bool modeChanged = _currentSortMode != mode;
        _currentSortMode = mode;
        _model.SortMode = mode.ToString();

        _model.Items = mode switch
        {
            SpaceSortMode.Name => _model.Items
                .OrderBy(i => i.DisplayName, StringComparer.OrdinalIgnoreCase)
                .ThenBy(i => i.SortOrder)
                .ToList(),

            SpaceSortMode.Type => _model.Items
                .OrderBy(i => i.IsDirectory ? 0 : 1)
                .ThenBy(i => GetSortFileType(i), StringComparer.OrdinalIgnoreCase)
                .ThenBy(i => i.DisplayName, StringComparer.OrdinalIgnoreCase)
                .ToList(),

            SpaceSortMode.DateModified => _model.Items
                .OrderByDescending(GetLastWriteUtc)
                .ThenBy(i => i.DisplayName, StringComparer.OrdinalIgnoreCase)
                .ToList(),

            SpaceSortMode.Usage => _model.Items
                .OrderByDescending(i => i.OpenCount)
                .ThenByDescending(i => i.LastOpenedTime ?? DateTime.MinValue)
                .ThenBy(i => i.DisplayName, StringComparer.OrdinalIgnoreCase)
                .ToList(),

            _ => _model.Items
                .OrderBy(i => i.SortOrder)
                .ToList()
        };

        NormalizeSortOrder(_model.Items);
        _layout.Invalidate();
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);

        if (animate)
            _animations.OnSpaceResorted(ModelId);

        bool changed = !before.SequenceEqual(_model.Items.Select(i => i.Id));
        if (changed || modeChanged)
            ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public SpaceSortMode GetSortMode() => _currentSortMode;

    public int GetIconSize() => _iconSize;

    public void SetIconSize(int size, bool notifyManager = true)
    {
        int clamped = Math.Clamp(size, 16, 96);
        if (_iconSize == clamped && _model.IconSizeOverride == clamped)
            return;

        _iconSize = clamped;
        _model.IconSizeOverride = clamped;

        _layout.Invalidate();
        _scrollOffset = 0;
        UpdateScrollInfo(GetDisplayedItems().Count);
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);

        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);

        if (notifyManager)
            IconSizeChanged?.Invoke(this, clamped);
    }

    public int GetItemCount() => _model.Items.Count;

    public void SyncDesktopItemVisibility(bool hide)
    {
        _ = hide;
        // Ownership is now file-system based (Desktop <-> .spaces), so no visual-hide pass is required.
    }

    public int GetOpacityPercent() => _opacityPercent;

    public void SetOpacityPercent(int percent)
    {
        _opacityPercent = Math.Clamp(percent, 20, 100);
        if (!IsAlive)
            return;

        byte opacity = (byte)(255 * _opacityPercent / 100);
        Win32.SetLayeredWindowAttributes(Handle, 0, opacity, Win32.LWA_ALPHA);
    }

    /// <summary>Returns true when the proposed preview values differ from the current snapshot.</summary>
    public bool LivePreviewDiffers(string titlebarMode, string rollupMode, bool snapToGrid, bool portalEnabled,
        int opacityPercent, bool blurEnabled, int glassStrength, int iconSpacing)
    {
        return _livePreviewTitlebarMode != titlebarMode
            || _livePreviewRollupMode != rollupMode
            || _livePreviewSnapToGrid != snapToGrid
            || _livePreviewPortalEnabled != portalEnabled
            || _opacityPercent != opacityPercent
            || _livePreviewBlurEnabled != blurEnabled
            || _livePreviewGlassStrength != glassStrength
            || _livePreviewIconSpacing != Math.Clamp(iconSpacing, 2, 20);
    }

    public void ApplyLivePreview(string titlebarMode, string rollupMode, bool snapToGrid, bool portalEnabled, int opacityPercent,
        bool blurEnabled, int glassStrength, int iconSpacing)
    {
        _livePreviewTitlebarMode = titlebarMode;
        _livePreviewRollupMode = rollupMode;
        _livePreviewSnapToGrid = snapToGrid;
        _livePreviewPortalEnabled = portalEnabled;
        _livePreviewBlurEnabled = blurEnabled;
        _livePreviewGlassStrength = glassStrength;
        _livePreviewIconSpacing = Math.Clamp(iconSpacing, 2, 20);
        SetOpacityPercent(opacityPercent);
        RefreshBlurBackground();
        InvalidateContent();
    }

    public void SetPerSpaceTitlebarModeOverride(string value)
    {
        _model.SettingsOverrides.TitlebarModeOverride = value;
        InvalidateContent();
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpaceTitlebarModeOverride() => _model.SettingsOverrides.TitlebarModeOverride ?? "UseGlobal";

    public void SetPerSpaceRollupModeOverride(string value)
    {
        _model.SettingsOverrides.RollupModeOverride = value;
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpaceRollupModeOverride() => _model.SettingsOverrides.RollupModeOverride ?? "UseGlobal";

    public void SetPerSpaceSnapModeOverride(string value)
    {
        _model.SettingsOverrides.SnapModeOverride = value;
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpaceSnapModeOverride() => _model.SettingsOverrides.SnapModeOverride ?? "UseGlobal";

    public void SetPerSpaceBlurOverride(string value)
    {
        _model.SettingsOverrides.BlurEnabledOverride = value switch
        {
            "Enabled" => true,
            "Disabled" => false,
            _ => null,
        };
        RefreshBlurBackground();
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpaceBlurOverride() => _model.SettingsOverrides.BlurEnabledOverride switch
    {
        true => "Enabled",
        false => "Disabled",
        _ => "UseGlobal",
    };

    public void SetPerSpacePortalOverride(string value)
    {
        _model.SettingsOverrides.PortalEnabledOverride = value switch
        {
            "Enabled" => true,
            "Disabled" => false,
            _ => null,
        };
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpacePortalOverride() => _model.SettingsOverrides.PortalEnabledOverride switch
    {
        true => "Enabled",
        false => "Disabled",
        _ => "UseGlobal",
    };

    public void SetPerSpacePortalViewOverride(string value)
    {
        _model.SettingsOverrides.LiveFolderViewOverride = value;
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetPerSpacePortalViewOverride() =>
        string.IsNullOrWhiteSpace(_model.SettingsOverrides.LiveFolderViewOverride)
            ? AppSettingsRepository.Instance.Current.LiveFolderDefaultView
            : _model.SettingsOverrides.LiveFolderViewOverride!;

    public string GetIncludeRulesText() => string.Join(", ", _model.SettingsOverrides.IncludeRules);

    public string GetExcludeRulesText() => string.Join(", ", _model.SettingsOverrides.ExcludeRules);

    public void SetIncludeRulesText(string text)
    {
        _model.SettingsOverrides.IncludeRules = ParseRuleTokens(text);
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public void SetExcludeRulesText(string text)
    {
        _model.SettingsOverrides.ExcludeRules = ParseRuleTokens(text);
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public void CycleSpaceColorOverride()
    {
        string current = _model.BackgroundColorOverride ?? "";
        _model.BackgroundColorOverride = current switch
        {
            "" => "#FF3B4252",
            "#FF3B4252" => "#FF4C566A",
            "#FF4C566A" => "#FF2E3440",
            _ => null,
        };

        InvalidateContent();
        ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
    }

    public string GetSpaceColorPresetLabel()
    {
        return _model.BackgroundColorOverride switch
        {
            "#FF3B4252" => "Slate",
            "#FF4C566A" => "Steel",
            "#FF2E3440" => "Graphite",
            _ => "Default",
        };
    }

    public string GetEffectiveTitlebarMode()
    {
        string overrideValue = _model.SettingsOverrides.TitlebarModeOverride ?? "UseGlobal";
        if (!string.Equals(overrideValue, "UseGlobal", StringComparison.OrdinalIgnoreCase)
            && !string.IsNullOrWhiteSpace(overrideValue))
            return overrideValue;

        if (!string.IsNullOrWhiteSpace(_livePreviewTitlebarMode))
            return _livePreviewTitlebarMode;

        return AppSettingsRepository.Instance.Current.GlobalTitlebarMode;
    }

    private bool ShouldRenderTitlebar()
    {
        string mode = GetEffectiveTitlebarMode();
        if (string.Equals(mode, "Hidden", StringComparison.OrdinalIgnoreCase))
            return false;

        if (string.Equals(mode, "Mouseover", StringComparison.OrdinalIgnoreCase)
            || string.Equals(mode, "MouseoverOnly", StringComparison.OrdinalIgnoreCase))
        {
            return _isMouseInClient || _isDragging;
        }

        return true;
    }

    private void OnMouseMoveTracked()
    {
        if (_rolledUp && !_rolling)
        {
            string rollupMode = GetEffectiveRollupMode();
            if (string.Equals(rollupMode, "HoverToOpen", StringComparison.OrdinalIgnoreCase)
                || string.Equals(rollupMode, "Hover", StringComparison.OrdinalIgnoreCase))
            {
                ToggleRollUp();
            }
        }

        if (!_isMouseInClient)
        {
            _isMouseInClient = true;
            if (string.Equals(GetEffectiveTitlebarMode(), "Mouseover", StringComparison.OrdinalIgnoreCase)
                || string.Equals(GetEffectiveTitlebarMode(), "MouseoverOnly", StringComparison.OrdinalIgnoreCase))
            {
                InvalidateContent();
            }
        }

        if (!_trackingMouseLeave)
        {
            var tme = new Win32.TRACKMOUSEEVENT
            {
                cbSize = (uint)Marshal.SizeOf<Win32.TRACKMOUSEEVENT>(),
                dwFlags = Win32.TME_LEAVE,
                hwndTrack = Handle,
                dwHoverTime = 0,
            };

            if (Win32.TrackMouseEvent(ref tme))
                _trackingMouseLeave = true;
        }
    }

    private void OnMouseLeaveTracked()
    {
        _trackingMouseLeave = false;
        if (!_isMouseInClient)
            return;

        _isMouseInClient = false;
        if (string.Equals(GetEffectiveTitlebarMode(), "Mouseover", StringComparison.OrdinalIgnoreCase)
            || string.Equals(GetEffectiveTitlebarMode(), "MouseoverOnly", StringComparison.OrdinalIgnoreCase))
        {
            InvalidateContent();
        }
    }

    private string GetEffectiveRollupMode()
    {
        string overrideValue = _model.SettingsOverrides.RollupModeOverride ?? "UseGlobal";
        if (!string.Equals(overrideValue, "UseGlobal", StringComparison.OrdinalIgnoreCase)
            && !string.IsNullOrWhiteSpace(overrideValue))
            return overrideValue;

        if (!string.IsNullOrWhiteSpace(_livePreviewRollupMode))
            return _livePreviewRollupMode;

        AppSettings settings = AppSettingsRepository.Instance.Current;
        if (settings.RollupRequiresHover)
            return "HoverToOpen";
        if (settings.RollupRequiresClick)
            return "ClickToOpen";

        return settings.GlobalRollupMode;
    }

    private bool GetEffectiveBlurEnabled()
    {
        if (_model.SettingsOverrides.BlurEnabledOverride.HasValue)
            return _model.SettingsOverrides.BlurEnabledOverride.Value;

        if (_livePreviewBlurEnabled.HasValue)
            return _livePreviewBlurEnabled.Value;

        return AppSettingsRepository.Instance.Current.BlurBackground;
    }

    private int GetEffectiveGlassStrength()
    {
        if (_livePreviewGlassStrength.HasValue)
            return _livePreviewGlassStrength.Value;

        return Math.Clamp(AppSettingsRepository.Instance.Current.GlassStrength, 0, 100);
    }

    private string GetEffectivePortalViewMode()
    {
        if (!string.IsNullOrWhiteSpace(_model.SettingsOverrides.LiveFolderViewOverride))
            return _model.SettingsOverrides.LiveFolderViewOverride!;

        string configured = AppSettingsRepository.Instance.Current.LiveFolderDefaultView;
        if (string.IsNullOrWhiteSpace(configured))
            return "Icons";

        return configured;
    }

    private bool ShouldSnapToEdges(AppSettings appSettings)
    {
        string mode = _model.SettingsOverrides.SnapModeOverride ?? "UseGlobal";
        return mode switch
        {
            "Snap" => true,
            "Free" => false,
            _ => appSettings.SnapToScreenEdges,
        };
    }

    private bool ShouldSnapToGrid(AppSettings appSettings)
    {
        string mode = _model.SettingsOverrides.SnapModeOverride ?? "UseGlobal";
        bool effectiveGlobal = _livePreviewSnapToGrid ?? appSettings.SnapToGrid;
        return mode switch
        {
            "Snap" => true,
            "Free" => false,
            _ => effectiveGlobal,
        };
    }

    // ── Search and Tab Management ─────────────────────────────────────────

    /// <summary>Set the search filter query and invalidate display.</summary>
    public void SetSearchQuery(string query)
    {
        _searchFilter.Query = query;
        _scrollOffset = 0;
        UpdateScrollInfo(GetDisplayedItems().Count);
        Win32.InvalidateRect(Handle, IntPtr.Zero, false);
        _layout.Invalidate(); // because filtered item count may change
    }

    /// <summary>Get currently displayed items (filtered by search if active).</summary>
    public IReadOnlyList<SpaceItemModel> GetDisplayedItems() =>
        _searchFilter.GetFilteredItems(_model.Items);

    /// <summary>Switch to adjacent tab by offset (-1 for prev, +1 for next).</summary>
    public bool SwitchTab(int offset)
    {
        bool switched = _tabManager.SwitchTab(offset);
        if (switched)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);
        return switched;
    }

    /// <summary>Toggle tab visibility in title bar.</summary>
    public void ToggleTabsVisible()
    {
        _tabManager.ToggleTabsVisible();
        Win32.InvalidateRect(Handle, IntPtr.Zero, false);
    }

    /// <summary>Invalidate the window and cached layout after model item updates.</summary>
    public void InvalidateContent()
    {
        if (_selectedItemId.HasValue && !_model.Items.Any(i => i.Id == _selectedItemId.Value))
            _selectedItemId = null;

        _layout.Invalidate();
        UpdateScrollInfo(GetDisplayedItems().Count);
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);
    }

    private void HandleItemDoubleClick(int lx, int ly)
    {
        IReadOnlyList<SpaceItemModel> displayed = GetDisplayedItems();
        int index = _layout.HitTestItem(lx, ly, displayed.Count, Width, Height, GetIconSize(), GetEffectiveIconSpacing(), _scrollOffset, GetTitleBarHeight());
        if (index < 0 || index >= displayed.Count)
            return;

        SpaceItemModel item = displayed[index];
        _selectedItemId = item.Id;
        if (string.IsNullOrWhiteSpace(item.TargetPath))
            return;

        try
        {
            if (!item.IsDirectory && !File.Exists(item.TargetPath) && !Directory.Exists(item.TargetPath))
            {
                item.IsUnresolved = true;
                ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
                return;
            }

            IntPtr result = Shell32.ShellExecute(Handle, "open", item.TargetPath, null, null, Win32.SW_SHOW);
            if (result.ToInt64() <= 32)
                throw new InvalidOperationException($"ShellExecute failed for '{item.TargetPath}' with code {result.ToInt64()}");

            item.IsUnresolved = false;
            SpaceUsageTracker.Instance.LogItemOpen(item);
            _animations.OnIconMoved(ModelId, item.Id);
            ReportDomainMutation(SpaceDomainMutationKind.ContentChanged);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "SpaceWindow '{Title}': failed opening item '{Path}'", Title, item.TargetPath);
        }
    }

    private void EvaluateSuggestionsAtPoint(int lx, int ly)
    {
        IReadOnlyList<SpaceItemModel> displayed = GetDisplayedItems();
        int index = _layout.HitTestItem(lx, ly, displayed.Count, Width, Height, GetIconSize(), GetEffectiveIconSpacing(), _scrollOffset, GetTitleBarHeight());
        if (index < 0 || index >= displayed.Count)
            return;

        SpaceItemModel item = displayed[index];
        IReadOnlyList<SpaceModel> allSpaces = SpaceStateService.Instance.Spaces;

        var suggestions = SpaceIconGroupingSuggester.Instance
            .GetSpaceSuggestions(item, allSpaces, topN: 3);

        if (suggestions.Count == 0)
            return;

        var best = suggestions[0];
        Log.Information(
            "AI Suggestion: move '{Item}' to '{Space}' (score {Score:0.00}) - {Reason}",
            item.DisplayName,
            best.Space.Title,
            best.ConfidenceScore,
            best.Reason);

        AnimationFeedbackService.Instance.PulseSuggestion(best.Space.Id, item.Id);
    }

    private void HandleRightClick(int lx, int ly)
    {
        ShowSpaceContextMenu(lx, ly);
    }

    private void HandleHoverMove(int lx, int ly)
    {
        IReadOnlyList<SpaceItemModel> displayed = GetDisplayedItems();
        int index = _layout.HitTestItem(lx, ly, displayed.Count, Width, Height, GetIconSize(), GetEffectiveIconSpacing(), _scrollOffset, GetTitleBarHeight());

        if (index < 0 || index >= displayed.Count)
        {
            if (_hoveredItemId.HasValue)
            {
                _hoveredItemId = null;
                HoverPreviewHidden?.Invoke();
            }
            return;
        }

        SpaceItemModel item = displayed[index];
        if (_hoveredItemId == item.Id)
            return;

        _hoveredItemId = item.Id;
        HoverPreviewData? preview = _hoverPreview.Build(item);
        if (preview != null)
            HoverPreviewChanged?.Invoke(preview);
    }

    private void HandleLeftButtonDown(int lx, int ly)
    {
        IReadOnlyList<SpaceItemModel> displayed = GetDisplayedItems();
        int index = _layout.HitTestItem(lx, ly, displayed.Count, Width, Height, GetIconSize(), GetEffectiveIconSpacing(), _scrollOffset, GetTitleBarHeight());
        if (index < 0 || index >= displayed.Count)
        {
            if (_selectedItemId.HasValue)
            {
                _selectedItemId = null;
                InvalidateContent();
            }
            return;
        }

        SpaceItemModel item = displayed[index];
        _selectedItemId = item.Id;
        SpaceUsageTracker.Instance.LogItemInteraction(item);
        ReportDomainMutation(SpaceDomainMutationKind.UsageChanged);

        InvalidateContent();

        _activeDrag = new DragPayload(Handle, item.Id);
        _isDragging = true;
        Win32.SetCapture(Handle);
    }

    private void HandleDragMove(int lx, int ly)
    {
        if (!_isDragging || _activeDrag == null)
            return;

        // Hook point: render drag ghost / highlight target space.
        _ = lx;
        _ = ly;
    }

    private void HandleLeftButtonUp(int lx, int ly)
    {
        if (_activeDrag == null)
            return;

        Win32.GetCursorPos(out Win32.POINT screenPt);
        ApplyDrop(this, lx, ly, screenPt);
    }

    private static void ApplyDrop(SpaceWindow fallbackTarget, int lx, int ly, Win32.POINT screenPt)
    {
        DragPayload? payload = _activeDrag;
        if (payload == null)
            return;

        if (!_instances.TryGetValue(payload.SourceHwnd, out SpaceWindow? source) || source == null)
        {
            ClearDragState();
            return;
        }

        SpaceItemModel? item = source._model.Items.FirstOrDefault(i => i.Id == payload.ItemId);
        if (item == null)
        {
            ClearDragState();
            return;
        }

        SpaceWindow? target = ResolveDropTarget(screenPt) ?? fallbackTarget;
        if (target == null)
        {
            ClearDragState();
            return;
        }

        if (source == target)
        {
            // Reorder inside same space.
            int fromIndex = source._model.Items.FindIndex(i => i.Id == item.Id);
            int toIndex = target._layout.HitTestItem(lx, ly, target._model.Items.Count, target.Width, target.Height, target.GetIconSize(), target.GetEffectiveIconSpacing(), target._scrollOffset, target.GetTitleBarHeight());
            if (fromIndex >= 0 && toIndex >= 0 && fromIndex != toIndex)
            {
                source._model.Items.RemoveAt(fromIndex);
                if (toIndex > source._model.Items.Count) toIndex = source._model.Items.Count;
                source._model.Items.Insert(toIndex, item);
                NormalizeSortOrder(source._model.Items);
                source.InvalidateContent();
                source.ReportDomainMutation(SpaceDomainMutationKind.OwnershipChanged);
            }

            ClearDragState();
            return;
        }

        // Move across spaces.
        source._model.Items.RemoveAll(i => i.Id == item.Id);

        if (target.IsAlive)
        {
            Win32.POINT local = screenPt;
            Win32.ScreenToClient(target.Handle, ref local);
            int insertIndex = target._layout.HitTestItem(local.x, local.y, target._model.Items.Count, target.Width, target.Height,
                target.GetIconSize(), target.GetEffectiveIconSpacing(), target._scrollOffset, target.GetTitleBarHeight());
            if (insertIndex < 0 || insertIndex > target._model.Items.Count)
                insertIndex = target._model.Items.Count;
            target._model.Items.Insert(insertIndex, item);
        }
        else
        {
            target._model.Items.Add(item);
        }

        NormalizeSortOrder(source._model.Items);
        NormalizeSortOrder(target._model.Items);

        source.InvalidateContent();
        target.InvalidateContent();
        target._animations.OnIconMoved(target.ModelId, item.Id);
        target.ReportDomainMutation(SpaceDomainMutationKind.OwnershipChanged);

        ClearDragState();
    }

    private static void NormalizeSortOrder(List<SpaceItemModel> items)
    {
        const int fallbackCols = 6;
        for (int i = 0; i < items.Count; i++)
        {
            items[i].SortOrder = i;
            items[i].GridColumn = i % fallbackCols;
            items[i].GridRow = i / fallbackCols;
        }
    }

    internal void SetExternalDropHover(bool active)
    {
        bool nextState = AppSettingsRepository.Instance.Current.HighlightDropTargets && active;
        if (_externalDropHover == nextState)
            return;

        _externalDropHover = nextState;
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);
    }

    internal void HandleOleDroppedFiles(IReadOnlyList<string> droppedPaths, Win32.POINT screenDropPoint)
    {
        if (!IsAlive || droppedPaths.Count == 0)
            return;

        var localDropPoint = screenDropPoint;
        Win32.ScreenToClient(Handle, ref localDropPoint);
        ImportDroppedPaths(droppedPaths, localDropPoint.x, localDropPoint.y);
    }

    private void HandleShellDrop(IntPtr hDrop)
    {
        try
        {
            int fileCount = (int)Shell32.DragQueryFile(hDrop, 0xFFFFFFFF, null, 0);
            if (fileCount <= 0)
                return;

            Win32.POINT? dropPoint = null;
            if (Shell32.DragQueryPoint(hDrop, out Win32.POINT queriedDropPoint))
                dropPoint = queriedDropPoint;

            var droppedPaths = new List<string>(fileCount);
            for (int i = 0; i < fileCount; i++)
            {
                var sb = new StringBuilder(1024);
                uint len = Shell32.DragQueryFile(hDrop, (uint)i, sb, (uint)sb.Capacity);
                if (len == 0)
                    continue;

                droppedPaths.Add(sb.ToString());
            }

            ImportDroppedPaths(droppedPaths, dropPoint?.x, dropPoint?.y);
        }
        finally
        {
            Shell32.DragFinish(hDrop);
        }
    }

    private void ImportDroppedPaths(IReadOnlyList<string> droppedPaths, int? dropClientX, int? dropClientY)
    {
        if (droppedPaths.Count == 0)
            return;

        int insertIndex = _model.Items.Count;
        if (dropClientX.HasValue && dropClientY.HasValue)
        {
            int dropIndex = _layout.HitTestItem(
                dropClientX.Value,
                dropClientY.Value,
                _model.Items.Count,
                Width,
                Height,
                GetIconSize(),
                GetEffectiveIconSpacing(),
                _scrollOffset,
                GetTitleBarHeight());

            if (dropIndex >= 0)
                insertIndex = dropIndex;
        }

        var newItems = new List<SpaceItemModel>();
        foreach (string sourcePath in droppedPaths)
        {
            bool isDirectory = Directory.Exists(sourcePath);
            bool isFile = File.Exists(sourcePath);
            if (!isDirectory && !isFile)
                continue;

            try
            {
                SpaceItemModel? imported = DragDropPolicyService.Instance.ImportIntoSpace(_model, sourcePath);
                if (imported == null)
                    continue;

                newItems.Add(imported);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "SpaceWindow '{Title}': drop import failed for '{Path}'", Title, sourcePath);
            }
        }

        if (newItems.Count == 0)
            return;

        insertIndex = Math.Clamp(insertIndex, 0, _model.Items.Count);
        foreach (SpaceItemModel newItem in newItems)
            _model.Items.Remove(newItem);
        _model.Items.InsertRange(insertIndex, newItems);
        NormalizeSortOrder(_model.Items);

        foreach (var dropped in newItems)
        {
            if (dropped.IsDirectory)
                continue;

            string ext = Path.GetExtension(dropped.TargetPath);
            if (string.IsNullOrWhiteSpace(ext))
                continue;

            BehaviorLearningService.Instance.RecordItemDroppedToSpace(ext, _model.Id, _model.Title);
        }

        _layout.Invalidate();
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);

        ReportDomainMutation(SpaceDomainMutationKind.OwnershipChanged);
        Log.Information("SpaceWindow '{Title}': imported {Count} dropped item(s)", Title, newItems.Count);
    }

    private void ShowSpaceContextMenu(int lx, int ly)
    {
        if (!IsAlive)
            return;

        IntPtr root = Shell32.CreatePopupMenu();
        IntPtr sortMenu = Shell32.CreatePopupMenu();
        IntPtr sizeMenu = Shell32.CreatePopupMenu();

        if (root == IntPtr.Zero || sortMenu == IntPtr.Zero || sizeMenu == IntPtr.Zero)
        {
            if (sizeMenu != IntPtr.Zero) Shell32.DestroyMenu(sizeMenu);
            if (sortMenu != IntPtr.Zero) Shell32.DestroyMenu(sortMenu);
            if (root != IntPtr.Zero) Shell32.DestroyMenu(root);
            return;
        }

        try
        {
            AddCheckedMenuItem(sortMenu, CmdSortManual, "Manual", _currentSortMode == SpaceSortMode.Manual);
            AddCheckedMenuItem(sortMenu, CmdSortName, "Name", _currentSortMode == SpaceSortMode.Name);
            AddCheckedMenuItem(sortMenu, CmdSortType, "Type", _currentSortMode == SpaceSortMode.Type);
            AddCheckedMenuItem(sortMenu, CmdSortDate, "Date Modified", _currentSortMode == SpaceSortMode.DateModified);
            AddCheckedMenuItem(sortMenu, CmdSortUsage, "Usage", _currentSortMode == SpaceSortMode.Usage);

            AddCheckedMenuItem(sizeMenu, CmdSize16, "16", _iconSize == 16);
            AddCheckedMenuItem(sizeMenu, CmdSize32, "32", _iconSize == 32);
            AddCheckedMenuItem(sizeMenu, CmdSize48, "48", _iconSize == 48);
            AddCheckedMenuItem(sizeMenu, CmdSize64, "64", _iconSize == 64);

            Shell32.AppendMenu(root, Shell32.MF_POPUP, (UIntPtr)sortMenu, "Sort By");
            Shell32.AppendMenu(root, Shell32.MF_POPUP, (UIntPtr)sizeMenu, "Icon Size");
            Shell32.AppendMenu(root, Shell32.MF_STRING, (UIntPtr)CmdAdjustSpacing, "Adjust Space Spacing...");
            AddCheckedMenuItem(root, CmdToggleLock, "Lock Space", IsLocked());
            Shell32.AppendMenu(root, Shell32.MF_SEPARATOR, UIntPtr.Zero, null);
            Shell32.AppendMenu(root, Shell32.MF_STRING, (UIntPtr)CmdRename, "Rename Space...");
            Shell32.AppendMenu(root, Shell32.MF_STRING, (UIntPtr)CmdDelete, "Delete Space...");

            var pt = new Win32.POINT { x = lx, y = ly };
            Win32.ClientToScreen(Handle, ref pt);

            Shell32.SetForegroundWindow(Handle);
            uint cmd = Shell32.TrackPopupMenu(root,
                Shell32.TPM_RETURNCMD | Shell32.TPM_RIGHTBUTTON | Shell32.TPM_LEFTALIGN,
                pt.x, pt.y, 0, Handle, IntPtr.Zero);

            if (cmd != 0)
                ExecuteContextMenuCommand(cmd);
        }
        finally
        {
            Shell32.DestroyMenu(root);
        }
    }

    private static void AddCheckedMenuItem(IntPtr menu, uint id, string label, bool isChecked)
    {
        uint flags = Shell32.MF_STRING | (isChecked ? Shell32.MF_CHECKED : 0u);
        Shell32.AppendMenu(menu, flags, (UIntPtr)id, label);
    }

    private void ExecuteContextMenuCommand(uint cmd)
    {
        switch (cmd)
        {
            case CmdSortManual:
                SortIcons(SpaceSortMode.Manual, animate: true);
                break;
            case CmdSortName:
                SortIcons(SpaceSortMode.Name, animate: true);
                break;
            case CmdSortType:
                SortIcons(SpaceSortMode.Type, animate: true);
                break;
            case CmdSortDate:
                SortIcons(SpaceSortMode.DateModified, animate: true);
                break;
            case CmdSortUsage:
                SortIcons(SpaceSortMode.Usage, animate: true);
                break;
            case CmdSize16:
                SetIconSize(16);
                break;
            case CmdSize32:
                SetIconSize(32);
                break;
            case CmdSize48:
                SetIconSize(48);
                break;
            case CmdSize64:
                SetIconSize(64);
                break;
            case CmdAdjustSpacing:
                AdjustInterSpaceSpacingFromPrompt();
                break;
            case CmdToggleLock:
                ToggleLocked();
                break;
            case CmdRename:
                RenameSpaceFromPrompt();
                break;
            case CmdDelete:
                DeleteSpaceWithConfirmation();
                break;
        }
    }

    private void AdjustInterSpaceSpacingFromPrompt()
    {
        int current = Math.Clamp(AppSettingsRepository.Instance.Current.InterSpaceSpacing, 0, 64);
        string? input = Win32InputDialog.Show(
            Handle,
            "Set inter-space spacing in pixels (0-64).",
            "Adjust Space Spacing",
            current.ToString());

        if (input == null || !int.TryParse(input, out int spacing))
            return;

        InterSpaceSpacingChangeRequested?.Invoke(Math.Clamp(spacing, 0, 64));
    }

    private void RenameSpaceFromPrompt()
    {
        string? input = Win32InputDialog.Show(Handle, "Enter new space name:", "Rename Space", Title);
        if (string.IsNullOrWhiteSpace(input))
            return;

        Rename(input);
    }

    private void DeleteSpaceWithConfirmation()
    {
        int result = Win32.MessageBox(Handle,
            "Are you sure you want to delete this space?",
            "Delete Space",
            Win32.MB_YESNO | Win32.MB_ICONQUESTION | Win32.MB_DEFBUTTON2);

        if (result == Win32.IDYES)
            DeleteRequested?.Invoke(this);
    }

    private static string GetSortFileType(SpaceItemModel item)
    {
        if (item.IsDirectory)
            return "folder";

        if (!string.IsNullOrWhiteSpace(item.TrackedFileType))
            return item.TrackedFileType!;

        string ext = Path.GetExtension(item.TargetPath);
        return string.IsNullOrWhiteSpace(ext) ? "other" : ext;
    }

    private static DateTime GetLastWriteUtc(SpaceItemModel item)
    {
        try
        {
            if (item.IsDirectory)
                return Directory.GetLastWriteTimeUtc(item.TargetPath);

            return File.GetLastWriteTimeUtc(item.TargetPath);
        }
        catch
        {
            return DateTime.MinValue;
        }
    }

    private static SpaceSortMode ParseSortMode(string? value)
    {
        if (!string.IsNullOrWhiteSpace(value)
            && Enum.TryParse(value, ignoreCase: true, out SpaceSortMode mode))
            return mode;

        return SpaceSortMode.Manual;
    }

    private static string[] BuildDesktopRoots()
    {
        var roots = new List<string>();

        void AddRoot(Environment.SpecialFolder folder)
        {
            string path = Environment.GetFolderPath(folder);
            if (!string.IsNullOrWhiteSpace(path))
                roots.Add(Path.GetFullPath(path).TrimEnd('\\'));
        }

        AddRoot(Environment.SpecialFolder.DesktopDirectory);
        AddRoot(Environment.SpecialFolder.CommonDesktopDirectory);

        return roots.Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private static bool IsDesktopManagedPath(string path)
    {
        return SpaceFileOwnershipService.Instance.IsDesktopManagedPath(path);
    }

    private static void TrySetDesktopItemHidden(string path, bool hide)
    {
        _ = path;
        _ = hide;
    }

    private static SpaceWindow? ResolveDropTarget(Win32.POINT screenPt)
    {
        IntPtr hwndAtCursor = Win32.WindowFromPoint(screenPt);
        if (hwndAtCursor != IntPtr.Zero && _instances.TryGetValue(hwndAtCursor, out SpaceWindow? target) && target != null)
            return target;

        if (_activeDrag != null && _instances.TryGetValue(_activeDrag.SourceHwnd, out SpaceWindow? src) && src != null)
        {
            // Dragging outside spaces: release item back to desktop and remove from source space.
            SpaceItemModel? item = src._model.Items.FirstOrDefault(i => i.Id == _activeDrag.ItemId);
            if (item != null)
            {
                src._model.Items.RemoveAll(i => i.Id == item.Id);
                SpaceFileOwnershipService.Instance.ReleaseSpaceItemToDesktop(item);
                NormalizeSortOrder(src._model.Items);
                src.InvalidateContent();
                src.ReportDomainMutation(SpaceDomainMutationKind.OwnershipChanged);
            }
        }

        return null;
    }

    private static void ClearDragState()
    {
        Win32.ReleaseCapture();
        if (_activeDrag != null && _instances.TryGetValue(_activeDrag.SourceHwnd, out SpaceWindow? src) && src != null)
            src._isDragging = false;
        _activeDrag = null;
    }

    private static List<string> ParseRuleTokens(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
            return new List<string>();

        return text
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Where(s => !string.IsNullOrWhiteSpace(s))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private void HandleVScroll(IntPtr wParam)
    {
        int request = Win32.LOWORD(wParam);
        int next = _scrollOffset;
        int pageDelta = Math.Max(Height - GetTitleBarHeight() - 16, 32);
        int lineDelta = Math.Max(GetIconSize() / 2, 12);

        switch (request)
        {
            case Win32.SB_LINEUP:
                next -= lineDelta;
                break;
            case Win32.SB_LINEDOWN:
                next += lineDelta;
                break;
            case Win32.SB_PAGEUP:
                next -= pageDelta;
                break;
            case Win32.SB_PAGEDOWN:
                next += pageDelta;
                break;
            case Win32.SB_TOP:
                next = 0;
                break;
            case Win32.SB_BOTTOM:
                next = GetMaxScrollOffset(GetDisplayedItems().Count);
                break;
            case Win32.SB_THUMBTRACK:
            case Win32.SB_THUMBPOSITION:
                var si = CreateScrollInfo();
                if (Win32.GetScrollInfo(Handle, Win32.SB_VERT, ref si))
                    next = si.nTrackPos;
                break;
        }

        SetScrollOffset(next);
    }

    private void ScrollBy(int delta)
    {
        SetScrollOffset(_scrollOffset + delta);
    }

    private void SetScrollOffset(int value)
    {
        int clamped = Math.Clamp(value, 0, GetMaxScrollOffset(GetDisplayedItems().Count));
        if (clamped == _scrollOffset)
            return;

        _scrollOffset = clamped;
        UpdateScrollInfo(GetDisplayedItems().Count);
        if (IsAlive)
            Win32.InvalidateRect(Handle, IntPtr.Zero, false);
    }

    private void UpdateScrollInfo(int itemCount)
    {
        if (!IsAlive)
            return;

        int contentHeight = _layout.GetContentHeight(itemCount, Width, GetIconSize(), GetEffectiveIconSpacing(), GetTitleBarHeight());
        int page = Math.Max(Height, 1);
        int maxScroll = Math.Max(0, contentHeight - page);
        _scrollOffset = Math.Clamp(_scrollOffset, 0, maxScroll);

        var si = CreateScrollInfo();
        si.nMin = 0;
        si.nMax = Math.Max(contentHeight - 1, 0);
        si.nPage = (uint)page;
        si.nPos = _scrollOffset;

        Win32.SetScrollInfo(Handle, Win32.SB_VERT, ref si, true);
    }

    private Win32.SCROLLINFO CreateScrollInfo()
    {
        return new Win32.SCROLLINFO
        {
            cbSize = (uint)Marshal.SizeOf<Win32.SCROLLINFO>(),
            fMask = Win32.SIF_ALL,
        };
    }

    private int GetMaxScrollOffset(int itemCount)
    {
        int contentHeight = _layout.GetContentHeight(itemCount, Width, GetIconSize(), GetEffectiveIconSpacing(), GetTitleBarHeight());
        return Math.Max(0, contentHeight - Math.Max(Height, 1));
    }

    private int GetTitleBarHeight()
    {
        return SpaceRenderer.GetScaledTitleBarHeight(Handle);
    }

    private int GetEffectiveIconSpacing()
    {
        if (_livePreviewIconSpacing.HasValue)
            return Math.Clamp(_livePreviewIconSpacing.Value, 2, 20);

        return Math.Clamp(AppSettingsRepository.Instance.Current.IconSpacing, 2, 20);
    }

    public static bool TryPostDesktopSync()
    {
        foreach (IntPtr hwnd in _instances.Keys)
        {
            if (Win32.IsWindow(hwnd))
                return Win32.PostMessage(hwnd, WmDesktopSync, IntPtr.Zero, IntPtr.Zero);
        }

        return false;
    }

    // ── Snap to edge ──────────────────────────────────────────────────────────

    /// <summary>
    /// Adjusts the proposed drag/resize RECT (from WM_MOVING or WM_SIZING lParam)
    /// in-place to snap to work-area edges and to the edges of every other live
    /// space.  Returns true when any adjustment was made.
    /// </summary>
    private static unsafe bool LiveSnapRect(
        Win32.RECT* proposed, Win32.RECT wa, IntPtr hwnd,
        SpaceWindow self, AppSettings settings, bool isResize)
    {
        int threshold = Math.Max(1, settings.SnapThreshold);
        int spacing   = Math.Clamp(settings.InterSpaceSpacing, 0, 64);
        bool snapped  = false;
        int  w        = proposed->right  - proposed->left;
        int  h        = proposed->bottom - proposed->top;

        if (self.ShouldSnapToEdges(settings))
        {
            // Horizontal (left/right)
            if      (Math.Abs(proposed->left  - wa.left)  <= threshold) { proposed->left  = wa.left;      if (!isResize) proposed->right  = proposed->left  + w; snapped = true; }
            else if (Math.Abs(proposed->right - wa.right) <= threshold) { proposed->right = wa.right;     if (!isResize) proposed->left   = proposed->right - w; snapped = true; }
            // Vertical (top/bottom)
            if      (Math.Abs(proposed->top    - wa.top)    <= threshold) { proposed->top    = wa.top;     if (!isResize) proposed->bottom = proposed->top    + h; snapped = true; }
            else if (Math.Abs(proposed->bottom - wa.bottom) <= threshold) { proposed->bottom = wa.bottom;  if (!isResize) proposed->top    = proposed->bottom - h; snapped = true; }
        }

        if (settings.SnapToOtherSpaces)
        {
            foreach (var (otherHwnd, other) in _instances)
            {
                if (otherHwnd == hwnd) continue;
                int ol = other.X, ot = other.Y, or_ = other.X + other.Width, ob = other.Y + other.Height;

                int targetLeft   = or_ + spacing;
                int targetRight  = ol - spacing;
                int targetTop    = ob + spacing;
                int targetBottom = ot - spacing;

                if      (Math.Abs(proposed->left  - targetLeft)   <= threshold) { proposed->left   = targetLeft;   if (!isResize) proposed->right  = proposed->left   + w; snapped = true; }
                else if (Math.Abs(proposed->right - targetRight)  <= threshold) { proposed->right  = targetRight;  if (!isResize) proposed->left   = proposed->right  - w; snapped = true; }
                if      (Math.Abs(proposed->top    - targetTop)   <= threshold) { proposed->top    = targetTop;    if (!isResize) proposed->bottom = proposed->top    + h; snapped = true; }
                else if (Math.Abs(proposed->bottom - targetBottom)<= threshold) { proposed->bottom = targetBottom; if (!isResize) proposed->top    = proposed->bottom - h; snapped = true; }
            }
        }

        return snapped;
    }

    /// <summary>
    /// If any space edge is within <paramref name="threshold"/> pixels of a
    /// work-area edge, snap the space flush with that edge and reposition the
    /// window.
    /// </summary>
    private static void SnapToEdge(SpaceWindow fw, Win32.RECT wa, int threshold)
    {
        int x = fw.X, y = fw.Y, w = fw.Width, h = fw.Height;
        bool moved = false;

        if      (Math.Abs(x     - wa.left)   <= threshold) { x = wa.left;       moved = true; }
        else if (Math.Abs(x + w - wa.right)  <= threshold) { x = wa.right  - w; moved = true; }
        if      (Math.Abs(y     - wa.top)    <= threshold) { y = wa.top;        moved = true; }
        else if (Math.Abs(y + h - wa.bottom) <= threshold) { y = wa.bottom - h; moved = true; }

        if (!moved) return;

        fw.X = x;
        fw.Y = y;
        Win32.SetWindowPos(fw.Handle, Win32.HWND_BOTTOM, x, y, w, h, Win32.SWP_NOACTIVATE);
    }

    private static void SnapToGrid(SpaceWindow fw, Win32.RECT wa, int grid)
    {
        int x = wa.left + (int)Math.Round((fw.X - wa.left) / (double)grid) * grid;
        int y = wa.top + (int)Math.Round((fw.Y - wa.top) / (double)grid) * grid;

        x = Math.Clamp(x, wa.left, Math.Max(wa.left, wa.right - fw.Width));
        y = Math.Clamp(y, wa.top, Math.Max(wa.top, wa.bottom - fw.Height));

        fw.X = x;
        fw.Y = y;
        Win32.SetWindowPos(fw.Handle, Win32.HWND_BOTTOM, x, y, fw.Width, fw.Height, Win32.SWP_NOACTIVATE);
    }

    /// <summary>
    /// Return the work area (excluding taskbar) of the monitor that currently
    /// contains the majority of the given window.  Falls back to the primary
    /// work area if <c>GetMonitorInfo</c> fails.
    /// </summary>
    private static Win32.RECT GetMonitorWorkArea(IntPtr hwnd)
    {
        IntPtr hMon = Win32.MonitorFromWindow(hwnd, Win32.MONITOR_DEFAULTTONEAREST);
        var info = new Win32.MONITORINFOEX { cbSize = (uint)Marshal.SizeOf<Win32.MONITORINFOEX>() };
        if (hMon != IntPtr.Zero && Win32.GetMonitorInfo(hMon, ref info))
            return info.rcWork;
        return DesktopHost.GetPrimaryWorkArea(); // fallback
    }

    private static void EnsureClassRegistered()
    {
        if (_classRegistered) return;

        _wndProcDelegate = WndProc;

        var wc = new Win32.WNDCLASSEX
        {
            cbSize        = (uint)Marshal.SizeOf<Win32.WNDCLASSEX>(),
            style         = Win32.CS_HREDRAW | Win32.CS_VREDRAW | Win32.CS_DBLCLKS,
            lpfnWndProc   = Marshal.GetFunctionPointerForDelegate<Win32.WndProc>(_wndProcDelegate),
            hInstance     = Win32.GetModuleHandle(null),
            hCursor       = Win32.LoadCursor(IntPtr.Zero, Win32.IDC_ARROW),
            hbrBackground = IntPtr.Zero, // we paint everything in WM_PAINT
            lpszClassName = ClassName,
        };

        if (!Win32.RegisterClassEx(ref wc))
        {
            int err = Marshal.GetLastWin32Error();
            if (err != 1410) // ERROR_CLASS_ALREADY_EXISTS is safe to ignore
                throw new InvalidOperationException($"SpaceWindow: RegisterClassEx failed ({err})");
        }

        _classRegistered = true;
    }
}

