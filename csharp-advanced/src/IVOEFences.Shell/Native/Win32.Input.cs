using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Native;

/// <summary>
/// Win32 mouse/keyboard hooks, hotkeys, and timer P/Invoke.
/// </summary>
internal static partial class Win32
{
    // ─────────────────────────────────────────────────────────────────────
    // Mouse hook constants (used by Input/MouseHook.cs)
    // ─────────────────────────────────────────────────────────────────────

    public const int WH_MOUSE_LL    = 14;
    public const int WH_KEYBOARD_LL = 13;
    public const int HC_ACTION      = 0;
    public const uint LLMHF_INJECTED = 0x00000001;

    [StructLayout(LayoutKind.Sequential)]
    public struct MSLLHOOKSTRUCT
    {
        public POINT   pt;
        public uint    mouseData;
        public uint    flags;
        public uint    time;
        public UIntPtr dwExtraInfo;
    }

    public delegate IntPtr HookProc(int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SetWindowsHookEx(int idHook, HookProc lpfn,
        IntPtr hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool UnhookWindowsHookEx(IntPtr hhk);

    [DllImport("user32.dll")]
    public static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode,
        IntPtr wParam, IntPtr lParam);

    // ─────────────────────────────────────────────────────────────────────
    // Hotkeys
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    public const uint MOD_ALT   = 0x0001;
    public const uint MOD_CTRL  = 0x0002;
    public const uint MOD_SHIFT = 0x0004;
    public const uint MOD_WIN   = 0x0008;

    public const uint VK_1 = 0x31;
    public const uint VK_2 = 0x32;
    public const uint VK_3 = 0x33;
    public const uint VK_F = 0x46;
    public const uint VK_K = 0x4B;
    public const uint VK_SPACE = 0x20;
    public const uint VK_C = 0x43;
    public const uint VK_N = 0x4E;
    public const uint VK_P = 0x50;
    public const uint VK_S = 0x53;

    // ─────────────────────────────────────────────────────────────────────
    // Timer
    // ─────────────────────────────────────────────────────────────────────

    [DllImport("user32.dll")]
    public static extern IntPtr SetTimer(IntPtr hWnd, IntPtr nIDEvent, uint uElapse, IntPtr lpTimerFunc);

    [DllImport("user32.dll")]
    public static extern bool KillTimer(IntPtr hWnd, IntPtr uIDEvent);
}
