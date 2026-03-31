using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Shell;

namespace IVOEFences.Core.Services;

/// <summary>
/// Manages global hotkeys for IVOE Fences.
/// Supports Peek™ (Win+V), Quick-hide (Win+H), and custom hotkeys.
/// 
/// Hotkeys are registered with Windows and delivered via WM_HOTKEY messages.
/// </summary>
public sealed class HotkeyService
{
    private static readonly Lazy<HotkeyService> _instance = new(() => new HotkeyService());
    public static HotkeyService Instance => _instance.Value;

    private readonly Dictionary<int, HotkeyDefinition> _registeredHotkeys = new();
    private int _nextHotkeyId = 1;

    public event EventHandler<HotkeyPressedEventArgs>? HotkeyPressed;

    private HotkeyService() { }

    /// <summary>
    /// Registers a hotkey with Windows.
    /// </summary>
    public int Register(IntPtr hWnd, uint modifiers, uint vkCode, string? name = null)
    {
        int id = _nextHotkeyId++;

        try
        {
            if (!NativeMethods.RegisterHotKey(hWnd, id, modifiers, vkCode))
            {
                Serilog.Log.Warning("Failed to register hotkey {Name} (mod={Mod}, vk={VK})", 
                    name ?? "unknown", modifiers, vkCode);
                return -1;
            }

            var def = new HotkeyDefinition
            {
                Id = id,
                WindowHandle = hWnd,
                Modifiers = modifiers,
                VKCode = vkCode,
                Name = name ?? $"Hotkey_{id}"
            };

            _registeredHotkeys[id] = def;
            Serilog.Log.Information("Registered hotkey: {Name} (id={Id}, mod={Mod}, vk={VK})", 
                def.Name, id, modifiers, vkCode);

            return id;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Error registering hotkey");
            return -1;
        }
    }

    /// <summary>
    /// Unregisters a previously registered hotkey.
    /// </summary>
    public void Unregister(int hotkeyId)
    {
        if (!_registeredHotkeys.TryGetValue(hotkeyId, out var def))
            return;

        try
        {
            NativeMethods.UnregisterHotKey(def.WindowHandle, hotkeyId);
            _registeredHotkeys.Remove(hotkeyId);
            Serilog.Log.Information("Unregistered hotkey: {Name} (id={Id})", def.Name, hotkeyId);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Error unregistering hotkey {Id}", hotkeyId);
        }
    }

    /// <summary>
    /// Called by the main window when it receives WM_HOTKEY message.
    /// </summary>
    public void OnHotkeyMessage(int hotkeyId)
    {
        if (!_registeredHotkeys.TryGetValue(hotkeyId, out var def))
            return;

        HotkeyPressed?.Invoke(this, new HotkeyPressedEventArgs
        {
            HotkeyId = hotkeyId,
            Name = def.Name,
            Modifiers = def.Modifiers,
            VKCode = def.VKCode
        });

        Serilog.Log.Debug("Hotkey pressed: {Name}", def.Name);
    }

    /// <summary>
    /// Unregisters all hotkeys.
    /// </summary>
    public void UnregisterAll()
    {
        var ids = _registeredHotkeys.Keys.ToList();
        foreach (var id in ids)
        {
            Unregister(id);
        }
    }

    /// <summary>
    /// Gets the definition of a registered hotkey.
    /// </summary>
    public HotkeyDefinition? GetDefinition(int hotkeyId)
    {
        _registeredHotkeys.TryGetValue(hotkeyId, out var def);
        return def;
    }

    /// <summary>
    /// Gets all registered hotkeys.
    /// </summary>
    public IReadOnlyCollection<HotkeyDefinition> GetAllHotkeys()
    {
        return _registeredHotkeys.Values.ToList();
    }

    // ── PREDEFINED HOTKEYS ──

    public int RegisterPeekHotkey(IntPtr hWnd)
    {
        // Win+V — Peek (show/toggle fence visibility)
        return Register(hWnd, NativeMethods.MOD_WIN, NativeMethods.VK_V, "Peek");
    }

    public int RegisterQuickHideHotkey(IntPtr hWnd)
    {
        // Win+H — Quick-hide (toggle hide all visible fences)
        return Register(hWnd, NativeMethods.MOD_WIN, NativeMethods.VK_H, "Quick-hide");
    }

    public int RegisterUndoHotkey(IntPtr hWnd)
    {
        // Ctrl+Z — Undo
        return Register(hWnd, NativeMethods.MOD_CTRL, NativeMethods.VK_Z, "Undo");
    }

    // ── HOTKEY DEFINITION ──

    public class HotkeyDefinition
    {
        public int Id { get; set; }
        public IntPtr WindowHandle { get; set; }
        public uint Modifiers { get; set; }
        public uint VKCode { get; set; }
        public string? Name { get; set; }

        public string DisplayName
        {
            get
            {
                var mods = new List<string>();
                if ((Modifiers & NativeMethods.MOD_CTRL) != 0) mods.Add("Ctrl");
                if ((Modifiers & NativeMethods.MOD_SHIFT) != 0) mods.Add("Shift");
                if ((Modifiers & NativeMethods.MOD_ALT) != 0) mods.Add("Alt");
                if ((Modifiers & NativeMethods.MOD_WIN) != 0) mods.Add("Win");

                string keyName = GetKeyName(VKCode);
                var parts = mods;
                parts.Add(keyName);

                return string.Join("+", parts);
            }
        }

        private static string GetKeyName(uint vkCode)
        {
            return vkCode switch
            {
                NativeMethods.VK_V => "V",
                NativeMethods.VK_H => "H",
                NativeMethods.VK_Z => "Z",
                NativeMethods.VK_C => "C",
                _ => $"VK_{vkCode:X2}"
            };
        }
    }

    // ── EVENT ARGS ──

    public class HotkeyPressedEventArgs : EventArgs
    {
        public int HotkeyId { get; set; }
        public string? Name { get; set; }
        public uint Modifiers { get; set; }
        public uint VKCode { get; set; }
    }
}
