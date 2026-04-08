using System.Drawing;
using IVOESpaces.Core.Plugins;

namespace IVOESpaces.Core.Services;

public sealed class ThemeService
{
    private static readonly Lazy<ThemeService> _instance = new(() => new ThemeService());
    private readonly Win32ThemeSystemService _themeSystem;

    public static ThemeService Instance => _instance.Value;

    private ThemeService()
    {
        _themeSystem = Win32ThemeSystemService.Instance;
    }

    public PluginThemeSnapshot GetCurrentTheme()
    {
        _themeSystem.Reload();
        ThemeEngine.ThemeMode mode = ThemeEngine.Instance.GetEffectiveThemeMode();
        Color accent = _themeSystem.GetAccentColor();
        string accentHex = $"#{accent.R:X2}{accent.G:X2}{accent.B:X2}";
        return new PluginThemeSnapshot(mode.ToString(), ThemeEngine.Instance.IsDarkMode, accentHex);
    }

    public IReadOnlyDictionary<string, string> GetSharedResources()
    {
        _themeSystem.Reload();
        return _themeSystem.GetResources();
    }
}
