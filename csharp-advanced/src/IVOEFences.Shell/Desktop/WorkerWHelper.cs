using IVOEFences.Shell.Native;
using Serilog;

namespace IVOEFences.Shell.Desktop;

/// <summary>
/// Discovers and caches the WorkerW HWND that sits behind all desktop icons.
/// Fence windows are parented to this handle so they appear on the desktop surface.
/// </summary>
internal static class WorkerWHelper
{
    private static IntPtr _workerW = IntPtr.Zero;

    /// <summary>
    /// Returns the WorkerW HWND, discovering it if not already cached.
    /// Call <see cref="Invalidate"/> after an Explorer restart to force rediscovery.
    /// </summary>
    public static IntPtr GetWorkerW()
    {
        if (_workerW != IntPtr.Zero && Win32.IsWindow(_workerW))
            return _workerW;

        _workerW = Discover();
        return _workerW;
    }

    /// <summary>Force the next call to <see cref="GetWorkerW"/> to rediscover the WorkerW.</summary>
    public static void Invalidate() => _workerW = IntPtr.Zero;

    private static IntPtr Discover()
    {
        // First attempt: the WorkerW may already exist (e.g. Explorer running normally).
        // Avoid sending the undocumented 0x052C spawn message unnecessarily.
        IntPtr workerW = FindWorkerW();
        if (workerW != IntPtr.Zero)
            return workerW;

        // Step 1: Send 0x052C to Progman — causes Explorer to spawn a WorkerW
        IntPtr progman = Win32.FindWindow("Progman", null);
        if (progman == IntPtr.Zero)
        {
            Log.Warning("WorkerWHelper: Progman not found");
            return IntPtr.Zero;
        }

        Win32.SendMessageTimeout(progman, 0x052C, UIntPtr.Zero, IntPtr.Zero,
            0x0002 /* SMTO_ABORTIFHUNG */, 1000, out _);

        // Step 2: Enumerate again now that Explorer has created the WorkerW
        workerW = FindWorkerW();

        if (workerW == IntPtr.Zero)
            Log.Warning("WorkerWHelper: WorkerW not found — is Explorer running?");
        else
            Log.Debug("WorkerWHelper: WorkerW = {Handle:X}", workerW);

        return workerW;
    }

    /// <summary>
    /// Single EnumWindows pass looking for the WorkerW that sits behind
    /// SHELLDLL_DefView.  Returns <see cref="IntPtr.Zero"/> if not found.
    /// Does NOT send the undocumented 0x052C spawn message.
    /// </summary>
    private static IntPtr FindWorkerW()
    {
        IntPtr workerW = IntPtr.Zero;

        Win32.EnumWindows((topHandle, _) =>
        {
            // A WorkerW that contains the desktop listview has a SHELLDLL_DefView child
            IntPtr shellDll = Win32.FindWindowEx(topHandle, IntPtr.Zero, "SHELLDLL_DefView", null);
            if (shellDll != IntPtr.Zero)
            {
                // Our target is the NEXT sibling of the icon-containing WorkerW
                workerW = Win32.FindWindowEx(IntPtr.Zero, topHandle, "WorkerW", null);
                return false; // stop enumeration
            }
            return true;
        }, IntPtr.Zero);

        return workerW;
    }

    /// <summary>
    /// Returns the SysListView32 desktop icon list handle when available.
    /// This allows precise integration with the actual Explorer desktop list view.
    /// </summary>
    public static IntPtr GetDesktopListView()
    {
        IntPtr listView = IntPtr.Zero;

        Win32.EnumWindows((topHandle, _) =>
        {
            IntPtr shellDll = Win32.FindWindowEx(topHandle, IntPtr.Zero, "SHELLDLL_DefView", null);
            if (shellDll == IntPtr.Zero)
                return true;

            listView = Win32.FindWindowEx(shellDll, IntPtr.Zero, "SysListView32", null);
            return listView == IntPtr.Zero;
        }, IntPtr.Zero);

        return listView;
    }
}
