namespace IVOESpaces.Core.Plugins;

public sealed record PluginThemeSnapshot(
    string ThemeMode,
    bool IsDarkMode,
    string AccentColorHex);
