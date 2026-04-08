using IVOESpaces.Shell.Native;
using Serilog;
using System;
using System.Threading;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Detects when a fullscreen application is in the foreground and raises
/// <see cref="FullscreenChanged"/> so callers can auto-hide spaces during gaming.
/// Polls every 2 seconds on a background thread; minimal CPU overhead.
/// </summary>
internal sealed class FullscreenMonitor : IDisposable
{
    private readonly Timer _timer;
    private bool _fullscreenActive;
    private bool _disposed;

    public event EventHandler<bool>? FullscreenChanged;

    public bool IsFullscreenActive => _fullscreenActive;

    public FullscreenMonitor()
    {
        _timer = new Timer(Poll, null, TimeSpan.FromSeconds(2), TimeSpan.FromSeconds(2));
    }

    private void Poll(object? state)
    {
        bool detected = IsFullscreenWindowInForeground();
        if (detected == _fullscreenActive)
            return;

        _fullscreenActive = detected;
        Log.Debug("FullscreenMonitor: fullscreen state changed → {State}", detected);
        FullscreenChanged?.Invoke(this, detected);
    }

    /// <summary>
    /// Returns true when the foreground window covers the entire primary screen
    /// AND is not the desktop shell (Progman / WorkerW).
    /// </summary>
    private static bool IsFullscreenWindowInForeground()
    {
        IntPtr hwnd = Win32.GetForegroundWindow();
        if (hwnd == IntPtr.Zero)
            return false;

        // Skip the desktop shell window — it always covers the screen
        if (IsDesktopWindow(hwnd))
            return false;

        if (!Win32.GetWindowRect(hwnd, out Win32.RECT wr))
            return false;

        int screenW = Win32.GetSystemMetrics(Win32.SM_CXSCREEN);
        int screenH = Win32.GetSystemMetrics(Win32.SM_CYSCREEN);

        // A window is considered fullscreen when it covers the entire primary display
        return wr.left <= 0
            && wr.top  <= 0
            && wr.right  >= screenW
            && wr.bottom >= screenH;
    }

    private static bool IsDesktopWindow(IntPtr hwnd)
    {
        const int MaxClass = 256;
        var sb = new System.Text.StringBuilder(MaxClass);
        if (Win32.GetClassName(hwnd, sb, MaxClass) == 0)
            return false;

        string cls = sb.ToString();
        return cls.Equals("Progman", StringComparison.OrdinalIgnoreCase)
            || cls.Equals("WorkerW", StringComparison.OrdinalIgnoreCase);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _timer.Dispose();
    }
}
