using System.Runtime.InteropServices;
using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Desktop;

/// <summary>
/// App-level DPI helpers that interpret raw interop data with resilient fallbacks.
/// </summary>
internal static class DpiService
{
    [DllImport("shcore.dll", SetLastError = false)]
    private static extern int GetDpiForMonitor(
        IntPtr hMonitor,
        Win32.MonitorDpiType dpiType,
        out uint dpiX,
        out uint dpiY);

    [DllImport("user32.dll")]
    private static extern IntPtr MonitorFromPoint(Win32.POINT pt, uint dwFlags);

    public static float GetDpiScaleForPoint(Win32.POINT pt)
    {
        IntPtr hMonitor = MonitorFromPoint(pt, Win32.MONITOR_DEFAULTTONEAREST);
        if (hMonitor == IntPtr.Zero)
            return 1.0f;

        int hr = GetDpiForMonitor(hMonitor, Win32.MonitorDpiType.EffectiveDpi, out uint dpiX, out _);
        return (hr == 0 && dpiX > 0) ? dpiX / 96.0f : 1.0f;
    }

    public static float GetDpiScaleForWindow(IntPtr hwnd)
    {
        IntPtr hMonitor = Win32.MonitorFromWindow(hwnd, Win32.MONITOR_DEFAULTTONEAREST);
        if (hMonitor != IntPtr.Zero)
        {
            int hr = GetDpiForMonitor(hMonitor, Win32.MonitorDpiType.EffectiveDpi, out uint dpiX, out _);
            if (hr == 0 && dpiX > 0)
                return dpiX / 96.0f;
        }

        uint windowDpi = Win32.GetDpiForWindow(hwnd);
        return (windowDpi > 0) ? windowDpi / 96.0f : 1.0f;
    }
}
