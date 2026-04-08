using System.Runtime.InteropServices;
using IVOESpaces.Core;
using Serilog;

namespace IVOESpaces.Shell.Native;

/// <summary>
/// A minimal, pure Win32 input dialog — no WinForms, no VB runtime, no COM.
/// Creates a popup window with a label, an Edit control, and OK/Cancel buttons,
/// runs its own local GetMessage loop (modal), and returns the entered string
/// (or null when the user cancels).
/// </summary>
internal static class Win32InputDialog
{
    private const string InputClass = AppIdentity.InternalName + "_InputDialog";
    private static bool _classRegistered;
    private static Win32.WndProc? _proc; // prevent GC

    private const int ID_EDIT   = 101;
    private const int ID_OK     = 102;
    private const int ID_CANCEL = 103;

    private const int Width  = 420;
    private const int Height = 180;

    private sealed class DialogState
    {
        public IntPtr Hwnd;
        public bool   Accepted;
        public string Result = string.Empty;
    }

    // One active dialog at a time (matches Win32 modal idiom).
    private static DialogState? _state;

    // ── Public API ──────────────────────────────────────────────────────────

    /// <summary>
    /// Show a modal input dialog. Blocks until the user clicks OK or Cancel.
    /// Returns the entered text on OK, or null on Cancel / close.
    /// </summary>
    public static string? Show(IntPtr owner, string prompt, string title, string defaultValue = "")
    {
        EnsureClassRegistered();

        // Center on owner (or screen if no owner)
        int cx = Win32.GetSystemMetrics(Win32.SM_CXSCREEN);
        int cy = Win32.GetSystemMetrics(Win32.SM_CYSCREEN);
        int x  = (cx - Width)  / 2;
        int y  = (cy - Height) / 2;

        if (owner != IntPtr.Zero && Win32.GetWindowRect(owner, out Win32.RECT wr))
        {
            x = wr.left + (wr.right  - wr.left - Width)  / 2;
            y = wr.top  + (wr.bottom - wr.top  - Height) / 2;
        }

        _state = new DialogState();

        IntPtr hwnd = Win32.CreateWindowEx(
            Win32.WS_EX_DLGMODALFRAME | Win32.WS_EX_TOPMOST,
            InputClass,
            title,
            Win32.WS_POPUP | Win32.WS_CAPTION | Win32.WS_SYSMENU | Win32.WS_VISIBLE,
            x, y, Width, Height,
            owner,
            IntPtr.Zero,
            Win32.GetModuleHandle(null),
            IntPtr.Zero);

        if (hwnd == IntPtr.Zero)
        {
            Log.Warning("Win32InputDialog: CreateWindowEx failed (err {E})", Marshal.GetLastWin32Error());
            _state = null;
            return null;
        }

        _state.Hwnd = hwnd;

        // Build child controls
        bool dark = IsDarkMode();
        BuildControls(hwnd, prompt, defaultValue, dark);

        Win32.SetForegroundWindow(hwnd);

        // Block the STA thread with a local message loop until the dialog is destroyed.
        Win32.MSG msg;
        while (Win32.GetMessage(out msg, IntPtr.Zero, 0, 0))
        {
            // Allow IsDialogMessage to handle Tab / Enter / Escape
            if (Win32.IsDialogMessage(hwnd, ref msg))
                continue;

            Win32.TranslateMessage(ref msg);
            Win32.DispatchMessage(ref msg);

            // Dialog was destroyed — exit the local loop
            if (!Win32.IsWindow(hwnd))
                break;
        }

        bool accepted = _state?.Accepted ?? false;
        string result  = _state?.Result  ?? string.Empty;
        _state = null;

        return accepted ? result : null;
    }

    // ── Control construction ────────────────────────────────────────────────

    private static void BuildControls(IntPtr hwnd, string prompt, string defaultValue, bool dark)
    {
        // Prompt label
        Win32.CreateWindowEx(
            0, "STATIC", prompt,
            Win32.WS_CHILD | Win32.WS_VISIBLE | Win32.SS_LEFT,
            14, 14, Width - 28, 26,
            hwnd, IntPtr.Zero, Win32.GetModuleHandle(null), IntPtr.Zero);

        // Edit control (single line)
        IntPtr hEdit = Win32.CreateWindowEx(
            Win32.WS_EX_CLIENTEDGE,
            "EDIT", defaultValue,
            Win32.WS_CHILD | Win32.WS_VISIBLE | Win32.ES_AUTOHSCROLL,
            14, 44, Width - 28, 30,
            hwnd, new IntPtr(ID_EDIT),
            Win32.GetModuleHandle(null), IntPtr.Zero);

        // Set initial focus to edit box and select all text
        Win32.SetFocus(hEdit);
        Win32.SendMessage(hEdit, Win32.EM_SETSEL, IntPtr.Zero, new IntPtr(-1));

        // OK button
        Win32.CreateWindowEx(
            0, "BUTTON", "OK",
            Win32.WS_CHILD | Win32.WS_VISIBLE | Win32.BS_DEFPUSHBUTTON,
            Width - 204, 100, 90, 30,
            hwnd, new IntPtr(ID_OK),
            Win32.GetModuleHandle(null), IntPtr.Zero);

        // Cancel button
        Win32.CreateWindowEx(
            0, "BUTTON", "Cancel",
            Win32.WS_CHILD | Win32.WS_VISIBLE | Win32.BS_PUSHBUTTON,
            Width - 108, 100, 90, 30,
            hwnd, new IntPtr(ID_CANCEL),
            Win32.GetModuleHandle(null), IntPtr.Zero);
    }

    // ── Window procedure ────────────────────────────────────────────────────

    private static IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        switch (msg)
        {
            case Win32.WM_COMMAND:
            {
                int id = Win32.LOWORD(wParam);
                switch (id)
                {
                    case ID_OK:
                        AcceptDialog(hwnd);
                        return IntPtr.Zero;

                    case ID_CANCEL:
                        CancelDialog(hwnd);
                        return IntPtr.Zero;
                }
                break;
            }

            case Win32.WM_KEYDOWN:
                if (wParam.ToInt32() == Win32.VK_RETURN)
                {
                    AcceptDialog(hwnd);
                    return IntPtr.Zero;
                }
                if (wParam.ToInt32() == Win32.VK_ESCAPE)
                {
                    CancelDialog(hwnd);
                    return IntPtr.Zero;
                }
                break;

            case Win32.WM_CLOSE:
                CancelDialog(hwnd);
                return IntPtr.Zero;

            case Win32.WM_PAINT:
            {
                IntPtr hdc = Win32.BeginPaint(hwnd, out Win32.PAINTSTRUCT ps);
                Win32.GetClientRect(hwnd, out Win32.RECT client);
                bool dark = IsDarkMode();
                uint bgColor = dark ? Win32.RGB(40, 40, 40) : Win32.RGB(245, 246, 251);
                IntPtr brush = Win32.CreateSolidBrush(bgColor);
                Win32.FillRect(hdc, ref client, brush);
                Win32.DeleteObject(brush);
                Win32.EndPaint(hwnd, ref ps);
                return IntPtr.Zero;
            }

            case Win32.WM_CTLCOLORSTATIC:
            {
                // Color for static labels
                bool dark = IsDarkMode();
                Win32.SetBkMode(lParam, Win32.TRANSPARENT);
                Win32.SetTextColor(lParam, dark ? Win32.RGB(220, 220, 220) : Win32.RGB(20, 20, 20));
                uint bgColor = dark ? Win32.RGB(40, 40, 40) : Win32.RGB(245, 246, 251);
                return (IntPtr)Win32.CreateSolidBrush(bgColor); // system frees this
            }

            case Win32.WM_DESTROY:
                // Post quit to unblock the local GetMessage loop
                Win32.PostQuitMessage(0);
                return IntPtr.Zero;
        }

        return Win32.DefWindowProc(hwnd, msg, wParam, lParam);
    }

    private static void AcceptDialog(IntPtr hwnd)
    {
        if (_state == null) return;

        IntPtr hEdit = Win32.GetDlgItem(hwnd, ID_EDIT);
        if (hEdit != IntPtr.Zero)
        {
            int len = Win32.GetWindowTextLength(hEdit);
            if (len > 0)
            {
                var sb = new System.Text.StringBuilder(len + 2);
                Win32.GetWindowText(hEdit, sb, sb.Capacity);
                _state.Result = sb.ToString();
            }
            else
            {
                _state.Result = string.Empty;
            }
        }

        _state.Accepted = true;
        Win32.DestroyWindow(hwnd);
    }

    private static void CancelDialog(IntPtr hwnd)
    {
        if (_state != null)
            _state.Accepted = false;

        Win32.DestroyWindow(hwnd);
    }

    // ── Class registration ──────────────────────────────────────────────────

    private static void EnsureClassRegistered()
    {
        if (_classRegistered) return;

        _proc = WndProc;

        var wc = new Win32.WNDCLASSEX
        {
            cbSize        = (uint)Marshal.SizeOf<Win32.WNDCLASSEX>(),
            style         = Win32.CS_HREDRAW | Win32.CS_VREDRAW,
            lpfnWndProc   = Marshal.GetFunctionPointerForDelegate<Win32.WndProc>(_proc),
            hInstance     = Win32.GetModuleHandle(null),
            hCursor       = Win32.LoadCursor(IntPtr.Zero, Win32.IDC_ARROW),
            hbrBackground = new IntPtr(Win32.COLOR_WINDOW + 1),
            lpszClassName = InputClass,
        };

        if (!Win32.RegisterClassEx(ref wc))
        {
            int err = Marshal.GetLastWin32Error();
            if (err != 1410) // ERROR_CLASS_ALREADY_EXISTS is OK
                Log.Error("Win32InputDialog: RegisterClassEx failed (err {E})", err);
        }

        _classRegistered = true;
    }

    private static bool IsDarkMode()
    {
        try
        {
            using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            return (int)(key?.GetValue("AppsUseLightTheme") ?? 1) == 0;
        }
        catch { return false; }
    }
}
