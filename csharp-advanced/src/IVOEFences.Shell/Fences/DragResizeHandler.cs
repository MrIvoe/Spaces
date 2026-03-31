using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Fences;

internal static class DragResizeHandler
{
    public static int HitTestResizeBorder(Win32.RECT wr, int mx, int my, int border = 6)
    {
        bool onLeft = mx < wr.left + border;
        bool onRight = mx > wr.right - border;
        bool onTop = my < wr.top + border;
        bool onBottom = my > wr.bottom - border;

        if (onLeft && onTop) return Win32.HTTOPLEFT;
        if (onRight && onTop) return Win32.HTTOPRIGHT;
        if (onLeft && onBottom) return Win32.HTBOTTOMLEFT;
        if (onRight && onBottom) return Win32.HTBOTTOMRIGHT;
        if (onTop) return Win32.HTTOP;
        if (onBottom) return Win32.HTBOTTOM;
        if (onLeft) return Win32.HTLEFT;
        if (onRight) return Win32.HTRIGHT;

        return Win32.HTCLIENT;
    }
}
