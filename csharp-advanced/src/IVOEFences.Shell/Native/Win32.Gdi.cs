using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Native;

/// <summary>
/// Win32 GDI painting, fonts, and double-buffering P/Invoke.
/// </summary>
internal static partial class Win32
{
    // ─────────────────────────────────────────────────────────────────────
    // GDI painting
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll")]
    public static extern IntPtr GetDC(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

    [DllImport("user32.dll")]
    public static extern IntPtr BeginPaint(IntPtr hWnd, out PAINTSTRUCT lpPaint);

    [DllImport("user32.dll")]
    public static extern bool EndPaint(IntPtr hWnd, ref PAINTSTRUCT lpPaint);

    [DllImport("gdi32.dll")]
    public static extern IntPtr CreateSolidBrush(uint crColor);

    [DllImport("gdi32.dll")]
    public static extern IntPtr CreatePen(int fnPenStyle, int nWidth, uint crColor);

    [DllImport("user32.dll")]
    public static extern bool FillRect(IntPtr hDC, ref RECT lprc, IntPtr hbr);

    [DllImport("user32.dll")]
    public static extern bool FrameRect(IntPtr hDC, ref RECT lprc, IntPtr hbr);

    [DllImport("gdi32.dll")]
    public static extern bool DeleteObject(IntPtr hObject);

    [DllImport("gdi32.dll")]
    public static extern IntPtr SelectObject(IntPtr hdc, IntPtr h);

    [DllImport("gdi32.dll")]
    public static extern int SetBkMode(IntPtr hdc, int iBkMode);

    [DllImport("gdi32.dll")]
    public static extern uint SetTextColor(IntPtr hdc, uint crColor);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int DrawText(IntPtr hDC, string lpchText, int nCount, ref RECT lpRect, uint uFormat);

    public const uint DT_LEFT       = 0x00000000;
    public const uint DT_CENTER     = 0x00000001;
    public const uint DT_RIGHT      = 0x00000002;
    public const uint DT_VCENTER    = 0x00000004;
    public const uint DT_SINGLELINE = 0x00000020;
    public const uint DT_WORDBREAK  = 0x00000010;
    public const uint DT_END_ELLIPSIS = 0x00008000;
    public const uint DT_NOPREFIX     = 0x00000800;

    // ─────────────────────────────────────────────────────────────────────
    // Layered windows + additional drawing
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetLayeredWindowAttributes(
        IntPtr hwnd, uint crKey, byte bAlpha, uint dwFlags);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateFont(
        int cHeight, int cWidth, int cEscapement, int cOrientation,
        int cWeight, uint bItalic, uint bUnderline, uint bStrikeOut,
        uint iCharSet, uint iOutPrecision, uint iClipPrecision,
        uint iQuality, uint iPitchAndFamily, string pszFaceName);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern int AddFontResourceEx(string name, uint fl, IntPtr pdv);

    public const uint FR_PRIVATE = 0x10;

    [DllImport("gdi32.dll")]
    public static extern bool RoundRect(
        IntPtr hdc,
        int nLeftRect, int nTopRect, int nRightRect, int nBottomRect,
        int nEllipseWidth, int nEllipseHeight);

    [DllImport("gdi32.dll")]
    public static extern bool MoveToEx(IntPtr hdc, int x, int y, IntPtr lpPoint);

    [DllImport("gdi32.dll")]
    public static extern bool LineTo(IntPtr hdc, int x, int y);

    [DllImport("gdi32.dll")]
    public static extern IntPtr GetStockObject(int fnObject);

    // ─────────────────────────────────────────────────────────────────────
    // GDI double buffering
    // ─────────────────────────────────────────────────────────────────────

    /// <summary>Create a memory DC compatible with the given DC.</summary>
    [DllImport("gdi32.dll")]
    public static extern IntPtr CreateCompatibleDC(IntPtr hdc);

    /// <summary>Create a bitmap compatible with the given DC.</summary>
    [DllImport("gdi32.dll")]
    public static extern IntPtr CreateCompatibleBitmap(IntPtr hdc, int cx, int cy);

    /// <summary>Block-transfer pixels from source DC to destination DC.</summary>
    [DllImport("gdi32.dll")]
    public static extern bool BitBlt(
        IntPtr hdc, int x, int y, int cx, int cy,
        IntPtr hdcSrc, int x1, int y1, int rop);

    /// <summary>Delete a memory DC created by <see cref="CreateCompatibleDC"/>.</summary>
    [DllImport("gdi32.dll")]
    public static extern bool DeleteDC(IntPtr hdc);

    /// <summary>Draw an icon into a DC, with optional sizing.</summary>
    [DllImport("user32.dll")]
    public static extern bool DrawIconEx(
        IntPtr hdc, int xLeft, int yTop, IntPtr hIcon,
        int cxWidth, int cyHeight, uint istepIfAniCur,
        IntPtr hbrFlickerFreeDraw, uint diFlags);

    /// <summary>Draw the icon in normal (non-highlighted, non-disabled) state.</summary>
    public const uint DI_NORMAL = 0x0003;
}
