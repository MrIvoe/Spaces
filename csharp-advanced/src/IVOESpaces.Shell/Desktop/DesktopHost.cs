using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell.Desktop;

/// <summary>
/// Manages the desktop surface — WorkerW acquisition and re-anchoring after
/// Explorer restarts. Uses a live window provider (set by SpaceManager) to
/// iterate current space windows instead of accumulating stale callbacks.
/// </summary>
internal sealed class DesktopHost
{
    private static readonly DesktopHost _instance = new();
    public static DesktopHost Instance => _instance;

    private IntPtr _workerW = IntPtr.Zero;
    internal Func<IntPtr> WorkerWProvider { get; set; } = WorkerWHelper.GetWorkerW;
    internal Func<IntPtr> DesktopListViewProvider { get; set; } = WorkerWHelper.GetDesktopListView;
    internal Action InvalidateWorkerWProvider { get; set; } = WorkerWHelper.Invalidate;

    /// <summary>
    /// Set by SpaceManager at startup. Returns all live SpaceWindow instances
    /// so OnExplorerRestarted can re-anchor them without stale callback lists.
    /// </summary>
    public Func<IEnumerable<Action<IntPtr>>>? ReAnchorTargets { get; set; }

    // Cached primary-monitor work area — SPI_GETWORKAREA is cheap but called
    // very frequently (WM_EXITSIZEMOVE, SpaceManager startup).  Invalidated
    // via InvalidateWorkArea() on WM_DISPLAYCHANGE.
    private Win32.RECT _workArea;
    private bool _workAreaCached;

    private DesktopHost() { }

    /// <summary>
    /// Get the current WorkerW handle, refreshing if stale.
    /// </summary>
    public IntPtr WorkerW
    {
        get
        {
            if (_workerW == IntPtr.Zero || !Win32.IsWindow(_workerW))
                _workerW = WorkerWProvider();
            return _workerW;
        }
    }

    /// <summary>
    /// Called by <see cref="ShellHost"/> when WM_TASKBARCREATED fires (Explorer restarted).
    /// Rediscovers WorkerW and re-anchors all live space windows.
    /// </summary>
    public void OnExplorerRestarted()
    {
        InvalidateWorkerWProvider();
        _workerW = WorkerWProvider();
        IntPtr listView = DesktopListViewProvider();
        IntPtr reAnchorHandle = _workerW != IntPtr.Zero ? _workerW : listView;
        int targetCount = ReAnchorTargets?.Invoke().Count() ?? 0;
        Log.Information(
            "DesktopHost: Explorer restarted — re-anchoring {TargetCount} space target(s) using handle={Handle:X}; WorkerW={WorkerW:X}; desktop ListView={ListView:X}",
            targetCount,
            reAnchorHandle,
            _workerW,
            listView);

        if (ReAnchorTargets is null) return;

        foreach (var reanchor in ReAnchorTargets())
        {
            try { reanchor(reAnchorHandle); }
            catch (Exception ex) { Log.Error(ex, "DesktopHost: re-anchor callback failed"); }
        }
    }

    /// <summary>
    /// Get the primary monitor work area via Win32 SPI_GETWORKAREA.
    /// The result is cached and refreshed only when the display configuration
    /// changes (see <see cref="InvalidateWorkArea"/>).
    /// </summary>
    public static Win32.RECT GetPrimaryWorkArea()
    {
        var inst = Instance;
        if (!inst._workAreaCached)
        {
            Win32.SystemParametersInfo(Win32.SPI_GETWORKAREA, 0, ref inst._workArea, 0);
            inst._workAreaCached = true;
        }
        return inst._workArea;
    }

    /// <summary>
    /// Mark the cached work area as stale.  Call from a WM_DISPLAYCHANGE handler
    /// so the next <see cref="GetPrimaryWorkArea"/> re-queries the OS.
    /// </summary>
    public void InvalidateWorkArea() => _workAreaCached = false;

    public IntPtr DesktopListView => DesktopListViewProvider();
}
