using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Native;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell;

/// <summary>
/// Controls fence visibility state: peek mode, toggle all, fullscreen auto-hide,
/// idle fade, and desktop page filtering.
/// Extracted from ShellHost to isolate visibility concerns.
/// </summary>
internal sealed class FenceVisibilityController : IDisposable
{
    private readonly FenceManager _fences;
    private readonly FullscreenMonitor _fullscreenMonitor;
    private readonly IdleModeService _idleModeService;
    private bool _allFencesVisible = true;
    private bool _hiddenForFullscreen;
    private bool _idleFadeActive;
    private bool _peekActive;
    private bool _subscribed;
    private IntPtr _peekTimerOwnerHwnd = IntPtr.Zero;
    private static readonly IntPtr PeekTimerId = new(9001);

    public bool AllFencesVisible => _allFencesVisible;

    public FenceVisibilityController(
        FenceManager fences,
        FullscreenMonitor fullscreenMonitor,
        IdleModeService idleModeService)
    {
        _fences = fences;
        _fullscreenMonitor = fullscreenMonitor;
        _idleModeService = idleModeService;
    }

    public void Subscribe()
    {
        if (_subscribed)
            return;

        _subscribed = true;
        _allFencesVisible = true;
        _fullscreenMonitor.FullscreenChanged += OnFullscreenChanged;
        _idleModeService.IdleStarted += OnIdleStarted;
        _idleModeService.IdleEnded += OnIdleEnded;
    }

    public void SetPeekTimerOwner(IntPtr hwnd)
    {
        _peekTimerOwnerHwnd = hwnd;
    }

    public void ToggleAllFencesVisibility()
    {
        if (AppSettingsRepository.Instance.Current.KeepFencesOnTopPeekMode)
        {
            TogglePeekMode();
            return;
        }

        _allFencesVisible = !_allFencesVisible;
        RefreshFenceVisibility();
        if (!_allFencesVisible)
            EndPeekMode();
        Log.Information("FenceVisibilityController: hotkey visibility toggle -> {State}",
            _allFencesVisible ? "visible" : "hidden");
    }

    public void TogglePeekMode()
    {
        if (_peekActive)
        {
            EndPeekMode();
            return;
        }

        _peekActive = true;
        _allFencesVisible = true;
        RecomputeVisibility();
        _fences.SetAllTopmost(true);

        int delayMs = Math.Max(100, AppSettingsRepository.Instance.Current.PeekDelayMs);
        Win32.SetTimer(_peekTimerOwnerHwnd, PeekTimerId, (uint)delayMs, IntPtr.Zero);
        Log.Information("FenceVisibilityController: Peek mode enabled for {Delay} ms", delayMs);
    }

    public void EndPeekMode()
    {
        if (!_peekActive)
            return;

        _peekActive = false;
        Win32.KillTimer(_peekTimerOwnerHwnd, PeekTimerId);
        _fences.SetAllTopmost(false);
        Log.Information("FenceVisibilityController: Peek mode disabled");
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

    public void RefreshFenceVisibility()
    {
        RecomputeVisibility();
    }

    public void RecomputeVisibility()
    {
        var settings = AppSettingsRepository.Instance.Current;
        bool baseVisible = _allFencesVisible && !_hiddenForFullscreen;

        _fences.ApplyDesktopPageVisibility(
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
            Log.Information("FenceVisibilityController: fullscreen app detected — fences hidden");
        }
        else if (!isFullscreen && _hiddenForFullscreen)
        {
            _hiddenForFullscreen = false;
            RecomputeVisibility();
            Log.Information("FenceVisibilityController: fullscreen app exited — fences restored");
        }
    }

    private void OnIdleStarted(object? sender, EventArgs e)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (settings.IdleThresholdSeconds <= 0)
            return;

        int fadeTo = Math.Clamp(settings.IdleFadeOpacity, 20, 100);
        _fences.SetAllOpacityPercent(fadeTo);
        _idleFadeActive = true;
        Log.Information("FenceVisibilityController: idle fade enabled ({Opacity}%)", fadeTo);
    }

    private void OnIdleEnded(object? sender, EventArgs e)
    {
        if (!_idleFadeActive)
            return;

        _idleFadeActive = false;
        RestoreConfiguredOpacity();
        Log.Information("FenceVisibilityController: idle fade cleared");
    }

    private void RestoreConfiguredOpacity()
    {
        _fences.SetAllOpacityPercent(Math.Clamp(
            AppSettingsRepository.Instance.Current.FenceOpacity,
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
