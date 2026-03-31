using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Native;

/// <summary>
/// DWM (Desktop Window Manager) P/Invoke — blur behind, acrylic, margins.
/// </summary>
internal static class DwmApi
{
    [DllImport("dwmapi.dll")]
    public static extern int DwmExtendFrameIntoClientArea(IntPtr hWnd, ref MARGINS pMarInset);

    [DllImport("dwmapi.dll")]
    public static extern int DwmSetWindowAttribute(IntPtr hwnd, uint dwAttribute, ref int pvAttribute, int cbAttribute);

    [DllImport("dwmapi.dll")]
    public static extern bool DwmIsCompositionEnabled(out bool pfEnabled);

    [StructLayout(LayoutKind.Sequential)]
    public struct MARGINS
    {
        public int cxLeftWidth;
        public int cxRightWidth;
        public int cyTopHeight;
        public int cyBottomHeight;
    }

    // DwmSetWindowAttribute constants
    public const uint DWMWA_NCRENDERING_POLICY     = 2;
    public const uint DWMWA_TRANSITIONS_FORCEDISABLED = 3;
    public const uint DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    public const uint DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    public const uint DWMWA_BORDER_COLOR            = 34;
    public const uint DWMWA_CAPTION_COLOR           = 35;

    // Corner preference values
    public const int DWMWCP_DEFAULT    = 0;
    public const int DWMWCP_DONOTROUND = 1;
    public const int DWMWCP_ROUND      = 2;
    public const int DWMWCP_ROUNDSMALL = 3;

    /// <summary>
    /// Apply acrylic blur-behind to the fence window.
    /// On Windows 11 this uses <see cref="ACCENT_ENABLE_ACRYLICBLURBEHIND"/> with an
    /// 80 % opaque dark tint; on Windows 10 it falls back to <see cref="ACCENT_ENABLE_BLURBEHIND"/>.
    /// Also rounds OS-managed corners on Windows 11.
    /// </summary>
    /// <param name="darkMode">Whether the tint should favor a dark or light surface.</param>
    /// <param name="strengthPercent">Requested glass strength from 0-100.</param>
    public static void ApplyAcrylic(IntPtr hwnd, bool darkMode = true, int strengthPercent = 50)
    {
        uint tintArgb = BuildTintColor(darkMode, strengthPercent);

        // Round corners (Windows 11 — silently ignored on Windows 10)
        int round = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, ref round, sizeof(int));

        int immersiveDark = darkMode ? 1 : 0;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ref immersiveDark, sizeof(int));

        // Try acrylic (Win11 / Win10 1903+); fall back to plain blur on older builds
        bool acrylicOk = TrySetAccent(hwnd, ACCENT_ENABLE_ACRYLICBLURBEHIND, tintArgb);
        if (!acrylicOk)
            TrySetAccent(hwnd, ACCENT_ENABLE_BLURBEHIND, 0);
    }

    /// <summary>Disable the acrylic / blur-behind effect set by <see cref="ApplyAcrylic"/>.</summary>
    public static void RemoveAcrylic(IntPtr hwnd)
    {
        TrySetAccent(hwnd, ACCENT_DISABLED, 0);
    }

    private static uint BuildTintColor(bool darkMode, int strengthPercent)
    {
        int clamped = Math.Clamp(strengthPercent, 0, 100);
        byte alpha = (byte)(32 + (clamped * 172 / 100));
        byte red = darkMode ? (byte)28 : (byte)245;
        byte green = darkMode ? (byte)28 : (byte)245;
        byte blue = darkMode ? (byte)28 : (byte)245;
        return ((uint)alpha << 24) | ((uint)red << 16) | ((uint)green << 8) | blue;
    }

    private static unsafe bool TrySetAccent(IntPtr hwnd, int accentState, uint gradientColor)
    {
        var policy = new ACCENTPOLICY
        {
            AccentState   = accentState,
            AccentFlags   = 2,           // draw on all borders
            GradientColor = gradientColor,
        };
        var data = new WINDOWCOMPOSITIONATTRIBDATA
        {
            Attrib  = (uint)WCA_ACCENT_POLICY,
            pvData  = new IntPtr(&policy),
            cbData  = sizeof(ACCENTPOLICY),
        };
        return SetWindowCompositionAttribute(hwnd, ref data) == 0;
    }

    [DllImport("user32.dll")]
    public static extern int SetWindowCompositionAttribute(IntPtr hwnd, ref WINDOWCOMPOSITIONATTRIBDATA data);

    [StructLayout(LayoutKind.Sequential)]
    public struct WINDOWCOMPOSITIONATTRIBDATA
    {
        public uint   Attrib;
        public IntPtr pvData;
        public int    cbData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ACCENTPOLICY
    {
        public int AccentState;
        public int AccentFlags;
        public uint GradientColor;
        public int AnimationId;
    }

    public const int ACCENT_DISABLED                = 0;
    public const int ACCENT_ENABLE_BLURBEHIND       = 3;
    public const int ACCENT_ENABLE_ACRYLICBLURBEHIND = 4;
    public const int WCA_ACCENT_POLICY              = 19;
}
