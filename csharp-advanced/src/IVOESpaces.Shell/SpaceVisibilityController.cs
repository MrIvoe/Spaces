using IVOESpaces.Shell.Spaces;
using IVOESpaces.Shell.Native;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Controls space visibility state: peek mode, toggle all, fullscreen auto-hide,
/// idle fade, and desktop page filtering.
/// Extracted from ShellHost to isolate visibility concerns.
/// </summary>
internal sealed class SpaceVisibilityController : IDisposable
{
    private readonly SpaceManager _spaces;
    private readonly FullscreenMonitor _fullscreenMonitor;
    private readonly IdleModeService _idleModeService;
    private bool _allSpacesVisible = true;
    private bool _hiddenForFullscreen;
    private bool _idleFadeActive;
    private bool _peekActive;
    private bool _subscribed;
    private IntPtr _peekTimerOwnerHwnd = IntPtr.Zero;
    private static readonly IntPtr PeekTimerId = new(9001);

    public bool AllSpacesVisible => _allSpacesVisible;

    public SpaceVisibilityController(
        SpaceManager spaces,
        FullscreenMonitor fullscreenMonitor,
        IdleModeService idleModeService)
    {
        _spaces = spaces;
        _fullscreenMonitor = fullscreenMonitor;
        _idleModeService = idleModeService;
    }

    public void Subscribe()
    {
        if (_subscribed)
            return;

        _subscribed = true;
        _allSpacesVisible = true;
        _fullscreenMonitor.FullscreenChanged += OnFullscreenChanged;
        _idleModeService.IdleStarted += OnIdleStarted;
        _idleModeService.IdleEnded += OnIdleEnded;
    }

    public void SetPeekTimerOwner(IntPtr hwnd)
    {
        _peekTimerOwnerHwnd = hwnd;
    }

    public void ToggleAllSpacesVisibility()
    {
        if (AppSettingsRepository.Instance.Current.KeepSpacesOnTopPeekMode)
        {
            TogglePeekMode();
            return;
        }

        _allSpacesVisible = !_allSpacesVisible;
        RefreshSpaceVisibility();
        if (!_allSpacesVisible)
            EndPeekMode();
        Log.Information("SpaceVisibilityController: hotkey visibility toggle -> {State}",
            _allSpacesVisible ? "visible" : "hidden");
    }

    public void TogglePeekMode()
    {
        if (_peekActive)
        {
            EndPeekMode();
            return;
        }

        _peekActive = true;
        _allSpacesVisible = true;
        RecomputeVisibility();
        _spaces.SetAllTopmost(true);

        int delayMs = Math.Max(100, AppSettingsRepository.Instance.Current.PeekDelayMs);
        Win32.SetTimer(_peekTimerOwnerHwnd, PeekTimerId, (uint)delayMs, IntPtr.Zero);
        Log.Information("SpaceVisibilityController: Peek mode enabled for {Delay} ms", delayMs);
    }

    public void EndPeekMode()
    {
        if (!_peekActive)
            return;

        _peekActive = false;
        Win32.KillTimer(_peekTimerOwnerHwnd, PeekTimerId);
        _spaces.SetAllTopmost(false);
        Log.Information("SpaceVisibilityController: Peek mode disabled");
    }

    /// <summary>Returns true if the WM_TIMER message was consumed (was our peek timer on our owner window).</summary>
    public bool HandlePeekTimer(IntPtr hwnd, IntPtr wParam)
    {
        if (wParam != PeekTimerId)
            return false;

        if (_peekTimerOwnerHwnd != IntPtr.Zero && hwnd != _peekTimerOwnerHwnd)
            return false;

        EndPeekMode();
        return true;
    }

    public void RefreshSpaceVisibility()
    {
        RecomputeVisibility();
    }

    public void RecomputeVisibility()
    {
        var settings = AppSettingsRepository.Instance.Current;
        bool baseVisible = _allSpacesVisible && !_hiddenForFullscreen;

        _spaces.ApplyDesktopPageVisibility(
            baseVisible,
            settings.EnableDesktopPages,
            PageService.Instance.CurrentPageIndex);
    }

    public void ClearIdleFade()
    {
        if (!_idleFadeActive)
            return;

        _idleFadeActive = false;
        RestoreConfiguredOpacity();
    }

    private void OnFullscreenChanged(object? sender, bool isFullscreen)
    {
        if (!AppSettingsRepository.Instance.Current.AutoHideInFullscreenApps)
            return;

        if (isFullscreen && !_hiddenForFullscreen)
        {
            _hiddenForFullscreen = true;
            RecomputeVisibility();
            Log.Information("SpaceVisibilityController: fullscreen app detected — spaces hidden");
        }
        else if (!isFullscreen && _hiddenForFullscreen)
        {
            _hiddenForFullscreen = false;
            RecomputeVisibility();
            Log.Information("SpaceVisibilityController: fullscreen app exited — spaces restored");
        }
    }

    private void OnIdleStarted(object? sender, EventArgs e)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (settings.IdleThresholdSeconds <= 0)
            return;

        int fadeTo = Math.Clamp(settings.IdleFadeOpacity, 20, 100);
        _spaces.SetAllOpacityPercent(fadeTo);
        _idleFadeActive = true;
        Log.Information("SpaceVisibilityController: idle fade enabled ({Opacity}%)", fadeTo);
    }

    private void OnIdleEnded(object? sender, EventArgs e)
    {
        if (!_idleFadeActive)
            return;

        _idleFadeActive = false;
        RestoreConfiguredOpacity();
        Log.Information("SpaceVisibilityController: idle fade cleared");
    }

    private void RestoreConfiguredOpacity()
    {
        _spaces.SetAllOpacityPercent(Math.Clamp(
            AppSettingsRepository.Instance.Current.SpaceOpacity,
            20,
            100));
    }

    public void Dispose()
    {
        if (_subscribed)
        {
            _subscribed = false;
            _idleModeService.IdleStarted -= OnIdleStarted;
            _idleModeService.IdleEnded -= OnIdleEnded;
            _fullscreenMonitor.FullscreenChanged -= OnFullscreenChanged;
        }

        EndPeekMode();
    }
}
