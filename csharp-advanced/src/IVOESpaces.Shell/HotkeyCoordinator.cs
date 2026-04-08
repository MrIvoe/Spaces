using IVOESpaces.Shell.Native;
using IVOESpaces.Core.Services;
using Serilog;
using System.Runtime.InteropServices;

namespace IVOESpaces.Shell;

/// <summary>
/// Manages global hotkey registration, parsing, and dispatch.
/// Extracted from ShellHost to isolate hotkey concerns.
/// </summary>
internal sealed class HotkeyCoordinator : IDisposable
{
    private readonly List<int> _registeredHotkeys = new();
    private readonly Dictionary<int, Action> _hotkeyActions = new();

    public void Register(int id, uint modifiers, uint virtualKey, string name, Action action)
    {
        if (_hotkeyActions.ContainsKey(id))
        {
            Log.Warning("HotkeyCoordinator: duplicate hotkey id {Id} for {Name}", id, name);
            Win32.UnregisterHotKey(IntPtr.Zero, id);
            _registeredHotkeys.RemoveAll(existingId => existingId == id);
            _hotkeyActions.Remove(id);
        }

        if (Win32.RegisterHotKey(IntPtr.Zero, id, modifiers, virtualKey))
        {
            _registeredHotkeys.Add(id);
            _hotkeyActions[id] = action;
            Log.Information("HotkeyCoordinator: registered hotkey {Name} (id={Id})", name, id);
            return;
        }

        Log.Warning(
            "HotkeyCoordinator: failed to register hotkey {Name} (id={Id}, err={Err})",
            name, id, Marshal.GetLastWin32Error());
    }

    public void UnregisterAll()
    {
        foreach (int id in _registeredHotkeys)
            Win32.UnregisterHotKey(IntPtr.Zero, id);
        _registeredHotkeys.Clear();
        _hotkeyActions.Clear();
    }

    public void HandleHotkey(int id)
    {
        if (_hotkeyActions.TryGetValue(id, out Action? action))
            action();
    }

    public static bool TryParseHotkey(string? text, out uint modifiers, out uint virtualKey)
    {
        modifiers = 0;
        virtualKey = 0;
        if (string.IsNullOrWhiteSpace(text))
            return false;

        foreach (string rawPart in text.Split('+', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            string part = rawPart.Trim();
            if (part.Equals("Ctrl", StringComparison.OrdinalIgnoreCase) || part.Equals("Control", StringComparison.OrdinalIgnoreCase))
            { modifiers |= Win32.MOD_CTRL; continue; }
            if (part.Equals("Win", StringComparison.OrdinalIgnoreCase) || part.Equals("Windows", StringComparison.OrdinalIgnoreCase))
            { modifiers |= Win32.MOD_WIN; continue; }
            if (part.Equals("Alt", StringComparison.OrdinalIgnoreCase))
            { modifiers |= Win32.MOD_ALT; continue; }
            if (part.Equals("Shift", StringComparison.OrdinalIgnoreCase))
            { modifiers |= Win32.MOD_SHIFT; continue; }

            if (!TryParseVirtualKeyToken(part, out uint candidateKey))
                return false;

            // Exactly one non-modifier token is allowed.
            if (virtualKey != 0)
                return false;

            virtualKey = candidateKey;
            continue;

        }

        return modifiers != 0 && virtualKey != 0;
    }

    private static bool TryParseVirtualKeyToken(string token, out uint virtualKey)
    {
        virtualKey = 0;
        if (string.IsNullOrWhiteSpace(token))
            return false;

        if (token.Length == 1)
        {
            char c = char.ToUpperInvariant(token[0]);
            if (c >= 'A' && c <= 'Z')
            {
                virtualKey = c;
                return true;
            }

            if (c >= '0' && c <= '9')
            {
                virtualKey = c;
                return true;
            }
        }

        if (token.StartsWith("F", StringComparison.OrdinalIgnoreCase)
            && token.Length >= 2
            && int.TryParse(token[1..], out int fn)
            && fn >= 1 && fn <= 24)
        {
            virtualKey = (uint)(0x70 + (fn - 1));
            return true;
        }

        switch (token.ToUpperInvariant())
        {
            case "SPACE": virtualKey = 0x20; return true;
            case "LEFT": virtualKey = 0x25; return true;
            case "UP": virtualKey = 0x26; return true;
            case "RIGHT": virtualKey = 0x27; return true;
            case "DOWN": virtualKey = 0x28; return true;
            default: return false;
        }
    }

    public void RegisterStandardHotkeys(
        Action createSpace,
        Action toggleCollapseAll,
        Action cycleSortMode,
        Action<int> applyIconSize,
        Action toggleAllVisibility,
        Action searchAcrossSpaces,
        Action<int> switchDesktopPage,
        Action<int> switchWorkspace,
        Action commandPalette)
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableGlobalHotkeys)
        {
            Log.Information("HotkeyCoordinator: global hotkeys disabled by settings");
            return;
        }

        Register(1, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_N, "CreateSpace", createSpace);
        Register(2, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_C, "ToggleCollapseAll", toggleCollapseAll);
        Register(3, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_S, "CycleSortMode", cycleSortMode);
        Register(4, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_1, "IconSizeSmall", () => applyIconSize(32));
        Register(5, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_2, "IconSizeMedium", () => applyIconSize(48));
        Register(6, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_3, "IconSizeLarge", () => applyIconSize(64));

        if (TryParseHotkey(settings.ToggleHotkey, out uint toggleMods, out uint toggleVk))
            Register(7, toggleMods, toggleVk, "ToggleAllSpacesVisibility", toggleAllVisibility);
        else
            Register(7, Win32.MOD_WIN, Win32.VK_SPACE, "ToggleAllSpacesVisibility", toggleAllVisibility);

        if (TryParseHotkey(settings.SearchHotkey, out uint searchMods, out uint searchVk))
            Register(8, searchMods, searchVk, "SearchAcrossSpaces", searchAcrossSpaces);
        else
            Register(8, Win32.MOD_WIN, Win32.VK_F, "SearchAcrossSpaces", searchAcrossSpaces);

        Register(9, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_PRIOR, "DesktopPagePrevious", () => switchDesktopPage(-1));
        Register(10, Win32.MOD_CTRL | Win32.MOD_WIN, Win32.VK_NEXT, "DesktopPageNext", () => switchDesktopPage(1));

        Register(11, Win32.MOD_CTRL | Win32.MOD_ALT, Win32.VK_1, "SwitchWorkspace1", () => switchWorkspace(0));
        Register(12, Win32.MOD_CTRL | Win32.MOD_ALT, Win32.VK_2, "SwitchWorkspace2", () => switchWorkspace(1));
        Register(13, Win32.MOD_CTRL | Win32.MOD_ALT, Win32.VK_3, "SwitchWorkspace3", () => switchWorkspace(2));
        Register(14, Win32.MOD_CTRL | Win32.MOD_SHIFT, Win32.VK_P, "CommandPalette", commandPalette);
    }

    public void Dispose()
    {
        UnregisterAll();
    }
}
