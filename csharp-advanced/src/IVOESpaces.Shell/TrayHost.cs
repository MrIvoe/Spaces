using System.Runtime.InteropServices;
using IVOESpaces.Core;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Win32 system tray icon — implemented entirely with Shell_NotifyIcon.
/// No WinForms NotifyIcon, no Dispatcher.
/// 
/// The tray icon lives on a hidden message-only window that receives tray callbacks.
/// Menu items:
///   • Settings  (CMD_SETTINGS = 100)
///   • New Space (CMD_NEW_SPACE = 102)
///   • ─────────
///   • Exit       (CMD_EXIT    = 101)
/// </summary>
internal sealed class TrayHost
{
    private const string TrayWindowClass = AppIdentity.TrayWindowClass;
    private const uint   TrayIconId      = 1;

    private IntPtr _hwnd   = IntPtr.Zero;
    private IntPtr _hIcon  = IntPtr.Zero;
    private bool   _added;

    private static Win32.WndProc? _wndProcDelegate; // prevents GC
    private readonly Dictionary<uint, Action> _menuCommandRoutes;
    private readonly Dictionary<string, Action> _leftClickRoutes;

    // Raised when the user clicks "Exit" in the tray menu
    public event Action? ExitRequested;
    // Raised when the user clicks "Settings"
    public event Action? SettingsRequested;
    // Raised when the user clicks "New Space"
    public event Action? NewSpaceRequested;
    // Raised when the user left-clicks and action is ToggleSpaces
    public event Action? ToggleSpacesRequested;

    public IntPtr MessageWindowHandle => _hwnd;

    public TrayHost()
    {
        _menuCommandRoutes = new Dictionary<uint, Action>
        {
            [Shell32.CMD_EXIT] = () =>
            {
                Log.Information("TrayHost: Exit selected");
                ExitRequested?.Invoke();
            },
            [Shell32.CMD_SETTINGS] = () =>
            {
                Log.Information("TrayHost: Settings selected");
                SettingsRequested?.Invoke();
            },
            [Shell32.CMD_NEW_SPACE] = () =>
            {
                Log.Information("TrayHost: New Space selected");
                NewSpaceRequested?.Invoke();
            },
            [Shell32.CMD_TOGGLE_SPACES] = () =>
            {
                Log.Information("TrayHost: Toggle Spaces selected");
                ToggleSpacesRequested?.Invoke();
            },
        };

        _leftClickRoutes = new Dictionary<string, Action>(StringComparer.OrdinalIgnoreCase)
        {
            ["ToggleSpaces"] = () => ToggleSpacesRequested?.Invoke(),
            ["ShowMenu"] = ShowContextMenu,
            ["OpenSettings"] = () => SettingsRequested?.Invoke(),
        };
    }

    // ── Lifecycle ───────────────────────────────────────────────────────────

    public void Initialize()
    {
        RegisterWindowClass();
        CreateMessageWindow();
        LoadIcon();
        AddTrayIcon();
        Log.Information("TrayHost: tray icon added");
    }

    public void Dispose()
    {
        RemoveTrayIcon();
        if (_hIcon != IntPtr.Zero)
            Shell32.DestroyIcon(_hIcon);
        if (_hwnd != IntPtr.Zero)
            Win32.DestroyWindow(_hwnd);
        _hwnd = IntPtr.Zero;
    }

    // ── Re-create after Explorer restart ────────────────────────────────────

    public void RecreateIfNeeded()
    {
        if (_added) RemoveTrayIcon();
        AddTrayIcon();
        Log.Debug("TrayHost: tray icon recreated after Explorer restart");
    }

    // ── Private helpers ─────────────────────────────────────────────────────

    private void RegisterWindowClass()
    {
        _wndProcDelegate = TrayWndProc;

        var wc = new Win32.WNDCLASSEX
        {
            cbSize        = (uint)Marshal.SizeOf<Win32.WNDCLASSEX>(),
            lpfnWndProc   = Marshal.GetFunctionPointerForDelegate<Win32.WndProc>(_wndProcDelegate),
            hInstance     = Win32.GetModuleHandle(null),
            lpszClassName = TrayWindowClass,
        };

        if (!Win32.RegisterClassEx(ref wc))
        {
            int err = Marshal.GetLastWin32Error();
            if (err != 1410) // ERROR_CLASS_ALREADY_EXISTS is fine on re-init
                Log.Warning("TrayHost: RegisterClassEx error {Err}", err);
        }
    }

    private void CreateMessageWindow()
    {
        // HWND_MESSAGE (-3) = message-only window, invisible, no taskbar entry
        _hwnd = Win32.CreateWindowEx(
            0, TrayWindowClass, AppIdentity.ProductName + " TrayHost",
            0, 0, 0, 0, 0,
            new IntPtr(-3), // HWND_MESSAGE
            IntPtr.Zero, Win32.GetModuleHandle(null), IntPtr.Zero);

        if (_hwnd == IntPtr.Zero)
            Log.Error("TrayHost: failed to create message window (err {Err})",
                Marshal.GetLastWin32Error());
    }

    private void LoadIcon()
    {
        // Load the .ico file copied to the output directory under Resources/
        string iconPath = Path.Combine(AppContext.BaseDirectory, "Resources", "IVOESpaces.ico");

        if (File.Exists(iconPath))
        {
            _hIcon = Shell32.LoadImage(IntPtr.Zero, iconPath,
                Shell32.IMAGE_ICON, 16, 16,
                Shell32.LR_LOADFROMFILE | Shell32.LR_DEFAULTSIZE);
        }

        if (_hIcon == IntPtr.Zero)
        {
            // Fall back to the standard application icon (IDI_APPLICATION = 32512)
            _hIcon = Shell32.LoadImage(IntPtr.Zero, "#32512",
                Shell32.IMAGE_ICON, 0, 0, Shell32.LR_DEFAULTSIZE | Shell32.LR_SHARED);
            Log.Warning("TrayHost: custom icon not found at '{Path}' — using system default",
                iconPath);
        }
    }

    private void AddTrayIcon()
    {
        if (_hwnd == IntPtr.Zero) return;

        var nid = BuildNID();
        nid.uFlags         = Shell32.NIF_ICON | Shell32.NIF_MESSAGE | Shell32.NIF_TIP;
        nid.uCallbackMessage = Shell32.WM_TRAYICON;
        nid.hIcon          = _hIcon;
        nid.szTip          = AppIdentity.TrayTooltip;

        bool added = Shell32.Shell_NotifyIcon(Shell32.NIM_ADD, ref nid);
        if (!added)
        {
            Log.Error("TrayHost: failed to add tray icon");
            _added = false;
            return;
        }

        // Use version 4 so we get proper lParam event codes
        nid.uVersion = Shell32.NOTIFYICON_VERSION_4;
        bool versionOk = Shell32.Shell_NotifyIcon(Shell32.NIM_SETVERSION, ref nid);
        if (!versionOk)
            Log.Warning("TrayHost: failed to set tray icon version");

        _added = true;
    }

    private void RemoveTrayIcon()
    {
        if (!_added || _hwnd == IntPtr.Zero) return;
        var nid = BuildNID();
        bool removed = Shell32.Shell_NotifyIcon(Shell32.NIM_DELETE, ref nid);
        if (!removed)
            Log.Warning("TrayHost: failed to remove tray icon");
        _added = false;
    }

    private Shell32.NOTIFYICONDATA BuildNID()
    {
        return new Shell32.NOTIFYICONDATA
        {
            cbSize = (uint)Marshal.SizeOf<Shell32.NOTIFYICONDATA>(),
            hWnd   = _hwnd,
            uID    = TrayIconId,
            szTip  = string.Empty,
            szInfo = string.Empty,
            szInfoTitle = string.Empty,
        };
    }

    // ── Window procedure ────────────────────────────────────────────────────

    private IntPtr TrayWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == Shell32.WM_TRAYICON)
        {
            uint ev = (uint)(lParam.ToInt64() & 0xFFFF);

            if (ev == Win32.WM_RBUTTONUP || ev == Shell32.WM_CONTEXTMENU)
            {
                ShowContextMenu();
            }
            else if (ev == Win32.WM_LBUTTONUP)
            {
                string action = AppSettingsRepository.Instance.Current.TrayLeftClickAction;
                RouteLeftClickAction(action);
            }
            return IntPtr.Zero;
        }

        return Win32.DefWindowProc(hwnd, msg, wParam, lParam);
    }

    private void ShowContextMenu()
    {
        Log.Debug("TrayHost: showing context menu");
        Win32.GetCursorPos(out Win32.POINT pt);
        Log.Debug("TrayHost: cursor at ({X},{Y})", pt.x, pt.y);
        Shell32.SetForegroundWindow(_hwnd);

        IntPtr hMenu = Shell32.CreatePopupMenu();
        if (hMenu == IntPtr.Zero)
        {
            Log.Warning("TrayHost: CreatePopupMenu failed");
            return;
        }

        Shell32.AppendMenu(hMenu, Shell32.MF_STRING, (UIntPtr)Shell32.CMD_SETTINGS, "Settings");
        Shell32.AppendMenu(hMenu, Shell32.MF_STRING, (UIntPtr)Shell32.CMD_TOGGLE_SPACES, "Toggle Spaces");
        Shell32.AppendMenu(hMenu, Shell32.MF_STRING, (UIntPtr)Shell32.CMD_NEW_SPACE, "New Space");
        Shell32.AppendMenu(hMenu, Shell32.MF_SEPARATOR, UIntPtr.Zero, null);
        Shell32.AppendMenu(hMenu, Shell32.MF_STRING, (UIntPtr)Shell32.CMD_EXIT, "Exit");

        uint cmd = Shell32.TrackPopupMenu(hMenu,
            Shell32.TPM_RIGHTBUTTON | Shell32.TPM_LEFTALIGN | Shell32.TPM_RETURNCMD,
            pt.x, pt.y, 0, _hwnd, IntPtr.Zero);

        Log.Debug("TrayHost: menu returned command {Cmd}", cmd);
        Shell32.DestroyMenu(hMenu);

        RouteMenuCommand(cmd);
    }

    private void RouteLeftClickAction(string configuredAction)
    {
        if (_leftClickRoutes.TryGetValue(configuredAction, out Action? route))
        {
            route();
            return;
        }

        _leftClickRoutes["OpenSettings"]();
    }

    private void RouteMenuCommand(uint cmd)
    {
        if (_menuCommandRoutes.TryGetValue(cmd, out Action? route))
            route();
    }
}
