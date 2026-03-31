using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Native;

/// <summary>
/// Core Win32 P/Invoke declarations: windows, messages, GDI, hooks.
/// </summary>
internal static partial class Win32
{
    // ── Window styles ──────────────────────────────────────────────────────
    public const int WS_POPUP         = unchecked((int)0x80000000);
    public const int WS_VISIBLE       = 0x10000000;
    public const int WS_CHILD         = 0x40000000;
    public const int WS_CLIPSIBLINGS  = 0x04000000;
    public const int WS_EX_TOOLWINDOW = 0x00000080;
    public const int WS_EX_NOACTIVATE = 0x08000000;
    public const int WS_EX_LAYERED    = 0x00080000;
    public const int WS_EX_TRANSPARENT = 0x00000020;
    public const int WS_EX_APPWINDOW  = 0x00040000;

    // ── Class styles ───────────────────────────────────────────────────────
    public const uint CS_HREDRAW = 0x0002;
    public const uint CS_VREDRAW = 0x0001;
    public const uint CS_DBLCLKS = 0x0008;

    // ── Z-order ────────────────────────────────────────────────────────────
    public static readonly IntPtr HWND_BOTTOM    = new(1);
    public static readonly IntPtr HWND_TOP       = new(0);
    public static readonly IntPtr HWND_TOPMOST   = new(-1);
    public static readonly IntPtr HWND_NOTOPMOST = new(-2);

    // ── SetWindowPos flags ─────────────────────────────────────────────────
    public const uint SWP_NOACTIVATE   = 0x0010;
    public const uint SWP_SHOWWINDOW   = 0x0040;
    public const uint SWP_NOSIZE       = 0x0001;
    public const uint SWP_NOMOVE       = 0x0002;
    public const uint SWP_NOZORDER     = 0x0004;
    public const uint SWP_FRAMECHANGED = 0x0020;

    // ── ShowWindow ─────────────────────────────────────────────────────────
    public const int SW_SHOW = 5;
    public const int SW_HIDE = 0;
    public const int SW_SHOWNOACTIVATE = 4;

    // ── Window long indices ────────────────────────────────────────────────
    public const int GWL_STYLE   = -16;
    public const int GWL_EXSTYLE = -20;

    // ── Messages ───────────────────────────────────────────────────────────
    public const uint WM_DESTROY    = 0x0002;
    public const uint WM_PAINT      = 0x000F;
    public const uint WM_ERASEBKGND = 0x0014;
    public const uint WM_NCHITTEST  = 0x0084;
    public const uint WM_NCLBUTTONDOWN = 0x00A1;
    public const uint WM_LBUTTONDOWN   = 0x0201;
    public const uint WM_LBUTTONUP     = 0x0202;
    public const uint WM_LBUTTONDBLCLK = 0x0203;
    public const uint WM_RBUTTONUP     = 0x0205;
    public const uint WM_MOUSEMOVE     = 0x0200;
    public const uint WM_MOUSEWHEEL    = 0x020A;
    public const uint WM_VSCROLL       = 0x0115;
    public const uint WM_MOUSELEAVE    = 0x02A3;
    public const uint WM_HOTKEY        = 0x0312;
    public const uint WM_QUIT          = 0x0012;
    public const uint WM_SETTINGCHANGE = 0x001A;
    public const uint WM_THEMECHANGED  = 0x031A;
    public const uint WM_USER          = 0x0400;
    public const uint WM_APP           = 0x8000;

    public const uint VK_PRIOR         = 0x21;
    public const uint VK_NEXT          = 0x22;

    // ── Hit-test values ────────────────────────────────────────────────────
    public const int HTCAPTION = 2;
    public const int HTCLIENT  = 1;

    // ── GDI brushes / pens ─────────────────────────────────────────────────
    public const int SRCCOPY        = 0x00CC0020;
    public const int PS_SOLID       = 0;
    public const int TRANSPARENT    = 1;

    // ── System cursors ─────────────────────────────────────────────────────
    public const int IDC_ARROW = 32512;

    // ── Background brush ──────────────────────────────────────────────────
    public const int COLOR_WINDOW = 5;

    // ─────────────────────────────────────────────────────────────────────
    // Structs
    // ─────────────────────────────────────────────────────────────────────

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT
    {
        public int x, y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int left, top, right, bottom;
        public int Width  => right - left;
        public int Height => bottom - top;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MINMAXINFO
    {
        public POINT ptReserved;
        public POINT ptMaxSize;
        public POINT ptMaxPosition;
        public POINT ptMinTrackSize;
        public POINT ptMaxTrackSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MSG
    {
        public IntPtr hwnd;
        public uint   message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint   time;
        public POINT  pt;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WNDCLASSEX
    {
        public uint    cbSize;
        public uint    style;
        public IntPtr  lpfnWndProc;
        public int     cbClsExtra;
        public int     cbWndExtra;
        public IntPtr  hInstance;
        public IntPtr  hIcon;
        public IntPtr  hCursor;
        public IntPtr  hbrBackground;
        public string? lpszMenuName;
        public string  lpszClassName;
        public IntPtr  hIconSm;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PAINTSTRUCT
    {
        public IntPtr hdc;
        public bool   fErase;
        public RECT   rcPaint;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] rgbReserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SCROLLINFO
    {
        public uint cbSize;
        public uint fMask;
        public int nMin;
        public int nMax;
        public uint nPage;
        public int nPos;
        public int nTrackPos;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Window management
    // ─────────────────────────────────────────────────────────────────────

    /// <summary>Window procedure delegate (for RegisterClassEx / CreateWindowEx).</summary>
    public delegate IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool RegisterClassEx(ref WNDCLASSEX lpwcx);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateWindowEx(
        int    exStyle,
        string className,
        string? windowName,
        int    style,
        int    x, int y, int width, int height,
        IntPtr parent,
        IntPtr menu,
        IntPtr instance,
        IntPtr param);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool UpdateWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool InvalidateRect(IntPtr hWnd, IntPtr lpRect, bool bErase);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(
        IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy,
        uint uFlags);

    [DllImport("user32.dll")]
    public static extern IntPtr SetParent(IntPtr hWndChild, IntPtr hWndNewParent);

    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern int GetWindowLong(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtr", SetLastError = true)]
    public static extern IntPtr GetWindowLongPtr(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtr", SetLastError = true)]
    public static extern IntPtr SetWindowLongPtr(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll")]
    public static extern IntPtr DefWindowProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string lpClassName, string? lpWindowName);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(IntPtr hWndParent, IntPtr hWndChildAfter,
        string lpClassName, string? lpWindowName);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDesktopWindow();

    [DllImport("user32.dll")]
    public static extern bool GetCursorPos(out POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern IntPtr WindowFromPoint(POINT point);

    [DllImport("user32.dll")]
    public static extern IntPtr SetCapture(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ReleaseCapture();

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool ScreenToClient(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern int SetScrollInfo(IntPtr hwnd, int nBar, ref SCROLLINFO lpsi, bool redraw);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool GetScrollInfo(IntPtr hwnd, int nBar, ref SCROLLINFO lpsi);

    [StructLayout(LayoutKind.Sequential)]
    public struct TRACKMOUSEEVENT
    {
        public uint cbSize;
        public uint dwFlags;
        public IntPtr hwndTrack;
        public uint dwHoverTime;
    }

    public const uint TME_LEAVE = 0x00000002;

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool TrackMouseEvent(ref TRACKMOUSEEVENT lpEventTrack);

    // ─────────────────────────────────────────────────────────────────────
    // Message loop
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll")]
    public static extern bool GetMessage(out MSG lpMsg, IntPtr hWnd,
        uint wMsgFilterMin, uint wMsgFilterMax);

    [DllImport("user32.dll")]
    public static extern bool TranslateMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    public static extern IntPtr DispatchMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    public static extern void PostQuitMessage(int nExitCode);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int MessageBox(IntPtr hWnd, string lpText, string lpCaption, uint uType);

    // ─────────────────────────────────────────────────────────────────────
    // SendMessage
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SendMessageTimeout(
        IntPtr hWnd, uint Msg,
        UIntPtr wParam, IntPtr lParam,
        uint fuFlags, uint uTimeout,
        out UIntPtr lpdwResult);

    [DllImport("user32.dll")]
    public static extern uint RegisterWindowMessage(string lpString);

    // ─────────────────────────────────────────────────────────────────────
    // System info
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetModuleHandle(string? lpModuleName);

    [DllImport("user32.dll")]
    public static extern IntPtr LoadCursor(IntPtr hInstance, int lpCursorName);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int nIndex);

    public const int SM_CXSCREEN = 0;
    public const int SM_CYSCREEN = 1;
    public const int SM_CXDOUBLECLK = 36;
    public const int SM_CYDOUBLECLK = 37;

    [DllImport("user32.dll")]
    public static extern uint GetDoubleClickTime();

    [DllImport("user32.dll")]
    public static extern bool SystemParametersInfo(uint uiAction, uint uiParam,
        ref RECT pvParam, uint fWinIni);

    public const uint SPI_GETWORKAREA = 0x0030;

    // ─────────────────────────────────────────────────────────────────────
    // Additional window styles (Settings window)
    // ─────────────────────────────────────────────────────────────────────

    public const int WS_OVERLAPPED = 0x00000000;
    public const int WS_CAPTION    = 0x00C00000;
    public const int WS_SYSMENU    = 0x00080000;
    public const int WS_THICKFRAME = 0x00040000;

    // ShowWindow
    public const int SW_RESTORE = 9;

    // Messages
    public const uint WM_CLOSE          = 0x0010;
    public const uint WM_COMMAND        = 0x0111;
    public const uint WM_GETMINMAXINFO  = 0x0024;
    public const uint WM_EXITSIZEMOVE   = 0x0232;
    public const uint WM_DROPFILES      = 0x0233;
    public const uint WM_TIMER          = 0x0113;
    public const uint WM_SIZE           = 0x0005;
    public const uint WM_MOVING         = 0x0216;  // sent during move drag; lParam = RECT*
    public const uint WM_SIZING         = 0x0214;  // sent during resize drag; lParam = RECT*
    public const uint WM_APP_SETBOUNDS  = 0x8001;  // private: cross-thread SetWindowPos via PostMessage

    // WM_NCHITTEST return values
    public const int HTLEFT        = 10;
    public const int HTRIGHT       = 11;
    public const int HTTOP         = 12;
    public const int HTTOPLEFT     = 13;
    public const int HTTOPRIGHT    = 14;
    public const int HTBOTTOM      = 15;
    public const int HTBOTTOMLEFT  = 16;
    public const int HTBOTTOMRIGHT = 17;

    // Font weight / quality
    public const int FW_NORMAL = 400;
    public const int FW_BOLD   = 700;

    public const int DEFAULT_CHARSET     = 1;
    public const int OUT_DEFAULT_PRECIS  = 0;
    public const int CLIP_DEFAULT_PRECIS = 0;
    public const int ANTIALIASED_QUALITY = 4;
    public const int CLEARTYPE_QUALITY   = 5;
    public const int DEFAULT_PITCH       = 0;

    // GDI stock objects
    public const int NULL_BRUSH = 5;

    // Layered windows
    public const uint LWA_ALPHA    = 0x00000002;
    public const uint LWA_COLORKEY = 0x00000001;

    // ─────────────────────────────────────────────────────────────────────
    // Helper functions
    // ─────────────────────────────────────────────────────────────────────

    /// <summary>Create a GDI COLORREF from R, G, B components.</summary>
    public static uint RGB(byte r, byte g, byte b) =>
        (uint)(r | (g << 8) | (b << 16));

    /// <summary>Extract the signed X coordinate from a message lParam.</summary>
    public static int GET_X_LPARAM(IntPtr lp) =>
        (int)(short)((ulong)lp.ToInt64() & 0xFFFFUL);

    /// <summary>Extract the signed Y coordinate from a message lParam.</summary>
    public static int GET_Y_LPARAM(IntPtr lp) =>
        (int)(short)(((ulong)lp.ToInt64() >> 16) & 0xFFFFUL);

    // ─────────────────────────────────────────────────────────────────────
    // Additional P/Invoke (window management)
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    // ── NC double-click ──────────────────────────────────────────────────────
    /// <summary>Sent when the user double-clicks the title bar or any other non-client area.</summary>
    public const uint WM_NCLBUTTONDBLCLK = 0x00A3;

    // MessageBox flags and return values
    public const uint MB_OKCANCEL = 0x00000001;
    public const uint MB_YESNO = 0x00000004;
    public const uint MB_ICONQUESTION = 0x00000020;
    public const uint MB_DEFBUTTON2 = 0x00000100;
    public const int IDOK = 1;
    public const int IDYES = 6;

    public static int LOWORD(IntPtr value) => unchecked((short)((long)value & 0xFFFF));
    public static int HIWORD(IntPtr value) => unchecked((short)(((long)value >> 16) & 0xFFFF));

    // ── Scrollbar constants ──────────────────────────────────────────────────

    public const int SB_VERT = 1;

    public const uint SIF_RANGE = 0x0001;
    public const uint SIF_PAGE  = 0x0002;
    public const uint SIF_POS   = 0x0004;
    public const uint SIF_TRACKPOS = 0x0010;
    public const uint SIF_ALL   = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

    public const int SB_LINEUP = 0;
    public const int SB_LINEDOWN = 1;
    public const int SB_PAGEUP = 2;
    public const int SB_PAGEDOWN = 3;
    public const int SB_THUMBPOSITION = 4;
    public const int SB_THUMBTRACK = 5;
    public const int SB_TOP = 6;
    public const int SB_BOTTOM = 7;
    public const int SB_ENDSCROLL = 8;

    // ─────────────────────────────────────────────────────────────────────
    // Input dialog constants and P/Invoke (Win32InputDialog)
    // ─────────────────────────────────────────────────────────────────────

    // Extended window styles
    public const int WS_EX_DLGMODALFRAME = 0x00000001;
    public const int WS_EX_TOPMOST       = 0x00000008;
    public const int WS_EX_CLIENTEDGE    = 0x00000200;

    // Edit control styles
    public const int ES_AUTOHSCROLL = 0x0080;

    // Button control styles
    public const int BS_PUSHBUTTON    = 0x00000000;
    public const int BS_DEFPUSHBUTTON = 0x00000001;

    // Static control styles
    public const int SS_LEFT = 0x00000000;

    // Additional WM_ messages
    public const uint WM_KEYDOWN       = 0x0100;
    public const uint WM_CTLCOLORSTATIC = 0x0138;

    // Virtual keys
    public const int VK_RETURN = 0x0D;
    public const int VK_ESCAPE = 0x1B;

    // Edit control messages
    public const uint EM_SETSEL = 0x00B1;

    // ─────────────────────────────────────────────────────────────────────
    // Dialog + control P/Invoke
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll")]
    public static extern bool IsDialogMessage(IntPtr hDlg, ref MSG lpMsg);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SetFocus(IntPtr hWnd);
}
