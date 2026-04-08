using System.Runtime.InteropServices;
using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell.Input;

/// <summary>
/// WH_MOUSE_LL global mouse hook — pure Win32, no WinForms.
/// Fires <see cref="MouseDown"/>, <see cref="MouseUp"/>, <see cref="MouseMove"/>
/// events with screen coordinates.
/// 
/// Install on startup; Uninstall on exit.
/// </summary>
internal sealed class MouseHook : IDisposable
{
    // Low-level mouse messages
    private const int WM_LBUTTONDOWN = 0x0201;
    private const int WM_LBUTTONUP   = 0x0202;
    private const int WM_MOUSEMOVE   = 0x0200;

    private IntPtr _hook = IntPtr.Zero;
    private Win32.HookProc? _proc; // keep delegate alive
    private readonly object _lifecycleLock = new();
    private uint _lastLeftDownTime;
    private int _lastLeftDownX;
    private int _lastLeftDownY;

    public event Action<int, int>? MouseDown;
    public event Action<int, int>? MouseUp;
    public event Action<int, int>? MouseMove;
    public event Action<int, int>? MouseDoubleClick;

    public void Install()
    {
        lock (_lifecycleLock)
        {
            if (_hook != IntPtr.Zero)
                return;

            _proc = LowLevelMouseProc;
            _hook = Win32.SetWindowsHookEx(
                Win32.WH_MOUSE_LL,
                _proc,
                Win32.GetModuleHandle(null),
                0);

            if (_hook == IntPtr.Zero)
                Log.Error("MouseHook: SetWindowsHookEx failed (err {Err})",
                    Marshal.GetLastWin32Error());
            else
                Log.Debug("MouseHook: installed");
        }
    }

    public void Uninstall()
    {
        lock (_lifecycleLock)
        {
            if (_hook == IntPtr.Zero)
                return;

            bool ok = Win32.UnhookWindowsHookEx(_hook);
            if (!ok)
                Log.Warning("MouseHook: UnhookWindowsHookEx failed (err {Err})", Marshal.GetLastWin32Error());

            _hook = IntPtr.Zero;
            Log.Debug("MouseHook: uninstalled");
        }
    }

    public void Dispose() => Uninstall();

    private IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode == Win32.HC_ACTION)
        {
            var info = Marshal.PtrToStructure<Win32.MSLLHOOKSTRUCT>(lParam);

            // Skip injected/synthetic input to avoid feedback loops
            if ((info.flags & Win32.LLMHF_INJECTED) != 0)
                return Win32.CallNextHookEx(_hook, nCode, wParam, lParam);

            int x = info.pt.x;
            int y = info.pt.y;

            switch ((uint)wParam)
            {
                case WM_LBUTTONDOWN:
                    MouseDown?.Invoke(x, y);
                    DetectLeftDoubleClick(x, y, info.time);
                    break;
                case WM_LBUTTONUP:   MouseUp?.Invoke(x, y);   break;
                case WM_MOUSEMOVE:   MouseMove?.Invoke(x, y); break;
            }
        }

        return Win32.CallNextHookEx(_hook, nCode, wParam, lParam);
    }

    private void DetectLeftDoubleClick(int x, int y, uint time)
    {
        uint maxGapMs = Win32.GetDoubleClickTime();
        int maxDx = Math.Max(1, Win32.GetSystemMetrics(Win32.SM_CXDOUBLECLK) / 2);
        int maxDy = Math.Max(1, Win32.GetSystemMetrics(Win32.SM_CYDOUBLECLK) / 2);

        if (_lastLeftDownTime != 0)
        {
            uint dt = time >= _lastLeftDownTime ? time - _lastLeftDownTime : 0;
            bool closeInTime = dt <= maxGapMs;
            bool closeInSpace = Math.Abs(x - _lastLeftDownX) <= maxDx
                && Math.Abs(y - _lastLeftDownY) <= maxDy;

            if (closeInTime && closeInSpace)
            {
                MouseDoubleClick?.Invoke(x, y);
                _lastLeftDownTime = 0;
                return;
            }
        }

        _lastLeftDownTime = time;
        _lastLeftDownX = x;
        _lastLeftDownY = y;
    }
}
