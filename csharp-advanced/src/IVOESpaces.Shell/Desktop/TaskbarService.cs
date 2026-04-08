using System.Runtime.InteropServices;
using IVOESpaces.Shell.Native;

namespace IVOESpaces.Shell.Desktop;

/// <summary>
/// App-level taskbar interpretation with explicit fallback policy.
/// </summary>
internal static class TaskbarService
{
    public static (uint Edge, int Thickness) GetPrimaryTaskbarInfo()
    {
        try
        {
            var abd = new Win32.APPBARDATA { cbSize = (uint)Marshal.SizeOf<Win32.APPBARDATA>() };
            UIntPtr result = Win32.SHAppBarMessage(Win32.ABM_GETTASKBARPOS, ref abd);
            if (result != UIntPtr.Zero)
            {
                int thickness = abd.uEdge is Win32.ABE_LEFT or Win32.ABE_RIGHT
                    ? abd.rc.Width
                    : abd.rc.Height;
                return (abd.uEdge, Math.Max(thickness, 1));
            }
        }
        catch
        {
            // Keep default fallback below.
        }

        return (Win32.ABE_BOTTOM, 40);
    }
}
