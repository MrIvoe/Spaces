using System.Runtime.InteropServices;
using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Shell;

internal sealed class ShellMessageWindow : IDisposable
{
    private const string ClassName = "IVOEFences_ShellMessageWindow";
    private static Win32.WndProc? _wndProc;
    private static bool _registered;

    private readonly Action<uint, IntPtr, IntPtr> _handler;
    private IntPtr _hwnd;

    public IntPtr Handle => _hwnd;

    public ShellMessageWindow(Action<uint, IntPtr, IntPtr> handler)
    {
        _handler = handler;
    }

    public void Create()
    {
        if (_hwnd != IntPtr.Zero)
            return;

        if (!_registered)
        {
            _wndProc = WndProc;
            var wc = new Win32.WNDCLASSEX
            {
                cbSize = (uint)Marshal.SizeOf<Win32.WNDCLASSEX>(),
                lpfnWndProc = Marshal.GetFunctionPointerForDelegate(_wndProc),
                hInstance = Win32.GetModuleHandle(null),
                lpszClassName = ClassName,
            };

            Win32.RegisterClassEx(ref wc);
            _registered = true;
        }

        _hwnd = Win32.CreateWindowEx(
            0,
            ClassName,
            "IVOEFences Shell",
            0,
            0, 0, 0, 0,
            new IntPtr(-3),
            IntPtr.Zero,
            Win32.GetModuleHandle(null),
            IntPtr.Zero);
    }

    private IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        _handler(msg, wParam, lParam);
        return Win32.DefWindowProc(hwnd, msg, wParam, lParam);
    }

    public void Dispose()
    {
        if (_hwnd != IntPtr.Zero)
            Win32.DestroyWindow(_hwnd);

        _hwnd = IntPtr.Zero;
    }
}
