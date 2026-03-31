using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Native;

/// <summary>
/// Win32 multi-monitor, DPI, and shell/taskbar P/Invoke.
/// </summary>
internal static partial class Win32
{
    // ─────────────────────────────────────────────────────────────────────
    // Multi-monitor
    // ─────────────────────────────────────────────────────────────────────

    public const uint MONITOR_DEFAULTTONEAREST = 0x00000002;
    public const uint MONITORINFOF_PRIMARY     = 0x00000001;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MONITORINFOEX
    {
        public uint   cbSize;
        public RECT   rcMonitor;
        public RECT   rcWork;
        public uint   dwFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string szDevice;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint dwFlags);

    public delegate bool MonitorEnumProc(IntPtr hMonitor, IntPtr hdcMonitor, ref RECT lprcMonitor, IntPtr dwData);

    [DllImport("user32.dll")]
    public static extern bool EnumDisplayMonitors(
        IntPtr hdc,
        IntPtr lprcClip,
        MonitorEnumProc lpfnEnum,
        IntPtr dwData);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool GetMonitorInfo(IntPtr hMonitor, ref MONITORINFOEX lpmi);

    // ─────────────────────────────────────────────────────────────────────
    // Display/DPI change messages
    // ─────────────────────────────────────────────────────────────────────

    /// <summary>Sent when the display resolution, colour depth, or DPI changes.</summary>
    public const uint WM_DISPLAYCHANGE    = 0x007E;

    /// <summary>Sent when the effective DPI for a window changes.</summary>
    public const uint WM_DPICHANGED       = 0x02E0;

    /// <summary>Sent after a window has been moved or resized.</summary>
    public const uint WM_WINDOWPOSCHANGED = 0x0047;

    // ─────────────────────────────────────────────────────────────────────
    // Per-monitor DPI (shcore.dll — Windows 8.1+)
    // ─────────────────────────────────────────────────────────────────────

    /// <summary>
    /// MDT_EFFECTIVE_DPI: takes virtual-mode scaling into account — use for layout.
    /// MDT_ANGULAR_DPI / MDT_RAW_DPI: physical pixel ratios.
    /// </summary>
    public enum MonitorDpiType
    {
        EffectiveDpi = 0,
        AngularDpi   = 1,
        RawDpi       = 2,
    }

    [DllImport("shcore.dll", SetLastError = false)]
    private static extern int GetDpiForMonitor(
        IntPtr hMonitor,
        MonitorDpiType dpiType,
        out uint dpiX,
        out uint dpiY);

    // ─────────────────────────────────────────────────────────────────────
    // AppBar / taskbar detection (shell32.dll)
    // ─────────────────────────────────────────────────────────────────────

    public const uint ABM_NEW            = 0x00000000;
    public const uint ABM_REMOVE         = 0x00000001;
    public const uint ABM_QUERYPOS       = 0x00000002;
    public const uint ABM_SETPOS         = 0x00000003;
    public const uint ABM_GETSTATE       = 0x00000004;
    public const uint ABM_GETTASKBARPOS  = 0x00000005;
    public const uint ABM_ACTIVATE       = 0x00000006;
    public const uint ABM_GETAUTOHIDEBAR = 0x00000007;
    public const uint ABM_SETAUTOHIDEBAR = 0x00000008;

    public const uint ABE_LEFT    = 0;
    public const uint ABE_TOP     = 1;
    public const uint ABE_RIGHT   = 2;
    public const uint ABE_BOTTOM  = 3;

    [StructLayout(LayoutKind.Sequential)]
    public struct APPBARDATA
    {
        public uint  cbSize;
        public IntPtr hWnd;
        public uint  uCallbackMessage;
        public uint  uEdge;
        public RECT  rc;
        public IntPtr lParam;
    }

    [DllImport("shell32.dll")]
    public static extern UIntPtr SHAppBarMessage(uint dwMessage, ref APPBARDATA pData);

}
