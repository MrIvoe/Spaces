using System.Text.Json;
using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell.UI;

internal enum FluentIcon
{
    Apps,
    Keyboard,
    Grid,
    Flow,
    Desktop,
    PlugConnected,
    Wrench,
    PanelLeft,
    PanelLeftContract,
    LockClosed,
    BotSparkle,
    Folder,
    Search,
    ChevronRight,
}

internal static class FluentIcons
{
    private const string FontFileName = "FluentSystemIcons-Regular.ttf";
    private const string MapFileName = "FluentSystemIcons-Regular.json";
    private const string FontFace = "FluentSystemIcons-Regular";

    private static readonly object InitLock = new();
    private static bool _initialized;
    private static readonly Dictionary<string, int> GlyphMap = new(StringComparer.Ordinal);

    private static readonly Dictionary<FluentIcon, string> KeyByIcon = new()
    {
        [FluentIcon.Apps] = "ic_fluent_apps_20_regular",
        [FluentIcon.Keyboard] = "ic_fluent_keyboard_20_regular",
        [FluentIcon.Grid] = "ic_fluent_grid_20_regular",
        [FluentIcon.Flow] = "ic_fluent_flow_20_regular",
        [FluentIcon.Desktop] = "ic_fluent_desktop_20_regular",
        [FluentIcon.PlugConnected] = "ic_fluent_plug_connected_20_regular",
        [FluentIcon.Wrench] = "ic_fluent_wrench_20_regular",
        [FluentIcon.PanelLeft] = "ic_fluent_panel_left_20_regular",
        [FluentIcon.PanelLeftContract] = "ic_fluent_panel_left_contract_20_regular",
        [FluentIcon.LockClosed] = "ic_fluent_lock_closed_20_regular",
        [FluentIcon.BotSparkle] = "ic_fluent_bot_sparkle_20_regular",
        [FluentIcon.Folder] = "ic_fluent_folder_20_regular",
        [FluentIcon.Search] = "ic_fluent_search_20_regular",
        [FluentIcon.ChevronRight] = "ic_fluent_chevron_right_20_regular",
    };

    public static bool DrawGlyph(IntPtr hdc, FluentIcon icon, int x, int y, int sizePx, uint color)
    {
        EnsureInitialized();

        if (!TryGetGlyph(icon, out string glyph))
            return false;

        IntPtr font = Win32.CreateFont(
            Math.Max(8, sizePx), 0, 0, 0, Win32.FW_NORMAL, 0, 0, 0,
            (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
            (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
            (uint)Win32.DEFAULT_PITCH, FontFace);

        if (font == IntPtr.Zero)
            return false;

        IntPtr old = Win32.SelectObject(hdc, font);
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, color);

        var rect = new Win32.RECT
        {
            left = x,
            top = y,
            right = x + sizePx + 6,
            bottom = y + sizePx + 6,
        };

        Win32.DrawText(hdc, glyph, -1, ref rect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_NOPREFIX);
        Win32.SelectObject(hdc, old);
        Win32.DeleteObject(font);
        return true;
    }

    private static bool TryGetGlyph(FluentIcon icon, out string glyph)
    {
        glyph = string.Empty;

        if (!KeyByIcon.TryGetValue(icon, out string? key))
            return false;

        if (!GlyphMap.TryGetValue(key, out int codepoint))
            return false;

        glyph = char.ConvertFromUtf32(codepoint);
        return true;
    }

    private static void EnsureInitialized()
    {
        if (_initialized)
            return;

        lock (InitLock)
        {
            if (_initialized)
                return;

            string resourceRoot = Path.Combine(AppContext.BaseDirectory, "Resources", "Icons", "Fluent");
            string fontPath = Path.Combine(resourceRoot, FontFileName);
            string mapPath = Path.Combine(resourceRoot, MapFileName);

            if (!File.Exists(fontPath) || !File.Exists(mapPath))
            {
                Log.Warning("FluentIcons: required files missing at {Root}", resourceRoot);
                _initialized = true;
                return;
            }

            int addedFonts = Win32.AddFontResourceEx(fontPath, Win32.FR_PRIVATE, IntPtr.Zero);
            if (addedFonts <= 0)
                Log.Warning("FluentIcons: AddFontResourceEx returned {Count} for {Path}", addedFonts, fontPath);

            using var fs = File.OpenRead(mapPath);
            var map = JsonSerializer.Deserialize<Dictionary<string, int>>(fs);
            GlyphMap.Clear();
            if (map is not null)
            {
                foreach (var pair in map)
                    GlyphMap[pair.Key] = pair.Value;
            }

            _initialized = true;
            Log.Information("FluentIcons: loaded {GlyphCount} glyphs", GlyphMap.Count);
        }
    }
}
