using System.Runtime.InteropServices;
using System.Text;
using System.Drawing;

namespace IVOESpaces.Shell.Native;

/// <summary>
/// Shell32 P/Invoke — tray icon, file info, Shell_NotifyIcon.
/// </summary>
internal static class Shell32
{
    [StructLayout(LayoutKind.Sequential)]
    public struct SIZE
    {
        public int cx;
        public int cy;
    }

    [ComImport]
    [Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IShellItem
    {
    }

    [ComImport]
    [Guid("BCC18B79-BA16-442F-80C4-8A59C30C463B")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IShellItemImageFactory
    {
        [PreserveSig]
        int GetImage(SIZE size, uint flags, out IntPtr phbm);
    }

    public const uint SIIGBF_RESIZETOFIT = 0x00;
    public const uint SIIGBF_BIGGERSIZEOK = 0x01;
    public const uint SIIGBF_ICONONLY = 0x04;

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    private static extern void SHCreateItemFromParsingName(
        [MarshalAs(UnmanagedType.LPWStr)] string pszPath,
        IntPtr pbc,
        ref Guid riid,
        [MarshalAs(UnmanagedType.Interface)] out IShellItem ppv);

    [ComImport]
    [Guid("0000010e-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IDataObject
    {
    }

    [ComImport]
    [Guid("00000121-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IDropSource
    {
    }

    [ComImport]
    [Guid("00000122-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IDropTarget
    {
    }

    // ── Shell_NotifyIcon ───────────────────────────────────────────────────
    public const uint NIM_ADD    = 0x00000000;
    public const uint NIM_MODIFY = 0x00000001;
    public const uint NIM_DELETE = 0x00000002;
    public const uint NIM_SETVERSION = 0x00000004;

    public const uint NIF_MESSAGE = 0x00000001;
    public const uint NIF_ICON    = 0x00000002;
    public const uint NIF_TIP     = 0x00000004;
    public const uint NIF_STATE   = 0x00000008;
    public const uint NIF_INFO    = 0x00000010;

    public const uint NOTIFYICON_VERSION_4 = 4;

    // Tray callback messages
    public const uint WM_TRAYICON = Win32.WM_USER + 1;

    // Mouse events delivered via WM_TRAYICON (wParam = icon id, lParam = event)
    public const uint NIN_SELECT     = 0x0400;
    public const uint NIN_BALLOONUSERCLICK = 0x0405;
    public const uint WM_CONTEXTMENU = 0x007B;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct NOTIFYICONDATA
    {
        public uint   cbSize;
        public IntPtr hWnd;
        public uint   uID;
        public uint   uFlags;
        public uint   uCallbackMessage;
        public IntPtr hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string szTip;
        public uint   dwState;
        public uint   dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szInfo;
        public uint   uVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string szInfoTitle;
        public uint   dwInfoFlags;
        public Guid   guidItem;
        public IntPtr hBalloonIcon;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern bool Shell_NotifyIcon(uint dwMessage, ref NOTIFYICONDATA lpData);

    // ── Icon loading ───────────────────────────────────────────────────────
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr LoadImage(
        IntPtr hInst, string lpszName, uint uType,
        int cxDesired, int cyDesired, uint fuLoad);

    public const uint IMAGE_ICON  = 1;
    public const uint LR_LOADFROMFILE = 0x00000010;
    public const uint LR_DEFAULTSIZE  = 0x00000040;
    public const uint LR_SHARED       = 0x00008000;

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool DestroyIcon(IntPtr hIcon);

    [DllImport("user32.dll")]
    public static extern IntPtr LoadIcon(IntPtr hInstance, string lpIconName);

    // ── Tray popup menu ────────────────────────────────────────────────────
    public const uint TPM_RIGHTBUTTON = 0x0002;
    public const uint TPM_LEFTALIGN   = 0x0000;
    public const uint TPM_RETURNCMD   = 0x0100;

    [DllImport("user32.dll")]
    public static extern IntPtr CreatePopupMenu();

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool AppendMenu(IntPtr hMenu, uint uFlags, UIntPtr uIDNewItem, string? lpNewItem);

    [DllImport("user32.dll")]
    public static extern bool DestroyMenu(IntPtr hMenu);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint TrackPopupMenu(IntPtr hMenu, uint uFlags,
        int x, int y, int nReserved, IntPtr hWnd, IntPtr prcRect);

    public const uint MF_STRING    = 0x00000000;
    public const uint MF_POPUP     = 0x00000010;
    public const uint MF_CHECKED   = 0x00000008;
    public const uint MF_SEPARATOR = 0x00000800;
    public const uint MF_GRAYED    = 0x00000001;

    // Menu command IDs
    public const uint CMD_SETTINGS = 100;
    public const uint CMD_TOGGLE_SPACES = 103;
    public const uint CMD_NEW_SPACE = 102;
    public const uint CMD_EXIT     = 101;

    // ── SHGetFileInfo — shell icon extraction ────────────────────────────────
    public const uint SHGFI_ICON              = 0x000000100;
    public const uint SHGFI_SMALLICON         = 0x000000001;
    public const uint SHGFI_LARGEICON         = 0x000000000;
    public const uint SHGFI_USEFILEATTRIBUTES = 0x000000010;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct SHFILEINFO
    {
        public IntPtr hIcon;
        public int    iIcon;
        public uint   dwAttributes;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szDisplayName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
        public string szTypeName;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SHGetFileInfo(
        string         pszPath,
        uint           dwFileAttributes,
        ref SHFILEINFO psfi,
        uint           cbSizeFileInfo,
        uint           uFlags);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern uint PrivateExtractIcons(
        string szFileName,
        int nIconIndex,
        int cxIcon,
        int cyIcon,
        IntPtr[] phicon,
        uint[] piconid,
        uint nIcons,
        uint flags);

    // ── Shell file-drop (WM_DROPFILES) ─────────────────────────────────────
    [DllImport("shell32.dll")]
    public static extern void DragAcceptFiles(IntPtr hWnd, bool fAccept);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern uint DragQueryFile(IntPtr hDrop, uint iFile, StringBuilder? lpszFile, uint cch);

    [DllImport("shell32.dll")]
    public static extern bool DragQueryPoint(IntPtr hDrop, out Win32.POINT lppt);

    [DllImport("shell32.dll")]
    public static extern void DragFinish(IntPtr hDrop);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr ShellExecute(
        IntPtr hwnd,
        string? lpOperation,
        string lpFile,
        string? lpParameters,
        string? lpDirectory,
        int nShowCmd);

    [DllImport("ole32.dll")]
    public static extern int RegisterDragDrop(IntPtr hwnd, [MarshalAs(UnmanagedType.Interface)] object pDropTarget);

    [DllImport("ole32.dll")]
    public static extern int RevokeDragDrop(IntPtr hwnd);

    public static IntPtr TryGetHighQualityIcon(string path, int size)
    {
        if (string.IsNullOrWhiteSpace(path) || (!File.Exists(path) && !Directory.Exists(path)))
            return IntPtr.Zero;

        IShellItem? shellItem = null;
        IShellItemImageFactory? factory = null;
        IntPtr hBitmap = IntPtr.Zero;

        try
        {
            Guid iidShellItem = typeof(IShellItem).GUID;
            SHCreateItemFromParsingName(path, IntPtr.Zero, ref iidShellItem, out shellItem);
            factory = (IShellItemImageFactory)shellItem;

            var requested = new SIZE { cx = size, cy = size };
            int hr = factory.GetImage(requested, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, out hBitmap);
            if (hr != 0 || hBitmap == IntPtr.Zero)
                return IntPtr.Zero;

            using Bitmap bmp = Image.FromHbitmap(hBitmap);
            return bmp.GetHicon();
        }
        catch
        {
            return IntPtr.Zero;
        }
        finally
        {
            if (hBitmap != IntPtr.Zero)
                Win32.DeleteObject(hBitmap);

            if (factory != null)
                Marshal.ReleaseComObject(factory);
            if (shellItem != null)
                Marshal.ReleaseComObject(shellItem);
        }
    }
}
