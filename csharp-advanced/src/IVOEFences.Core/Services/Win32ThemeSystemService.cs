using System.Drawing;
using System.Text.Json;
using IVOEFences.Core;

namespace IVOEFences.Core.Services;

public sealed class Win32ThemeSystemService
{
    private sealed class ThemeManifestDto
    {
        public string? Name { get; set; }
        public string? Mode { get; set; }
        public string? AccentColor { get; set; }
        public Dictionary<string, string>? Resources { get; set; }
    }

    private static readonly Lazy<Win32ThemeSystemService> _instance = new(() => new Win32ThemeSystemService());

    public static Win32ThemeSystemService Instance => _instance.Value;

    private readonly object _lock = new();
    private string _themeName = "Default";
    private Dictionary<string, string> _resources = new(StringComparer.OrdinalIgnoreCase);

    private Win32ThemeSystemService()
    {
        Reload();
    }

    public string ThemeName
    {
        get
        {
            lock (_lock)
            {
                return _themeName;
            }
        }
    }

    public IReadOnlyDictionary<string, string> GetResources()
    {
        lock (_lock)
        {
            return new Dictionary<string, string>(_resources, StringComparer.OrdinalIgnoreCase);
        }
    }

    public string GetAccentColorHexOrDefault()
    {
        lock (_lock)
        {
            return _resources.TryGetValue("theme.accent", out string? accent)
                ? accent
                : "#0078D4";
        }
    }

    public void Reload()
    {
        lock (_lock)
        {
            _resources = BuildDefaults();
            _themeName = "Default";

            if (!File.Exists(AppPaths.Win32ThemeSystemManifest))
                return;

            try
            {
                string json = File.ReadAllText(AppPaths.Win32ThemeSystemManifest);
                ThemeManifestDto? manifest = JsonSerializer.Deserialize<ThemeManifestDto>(json);
                if (manifest == null)
                    return;

                _themeName = string.IsNullOrWhiteSpace(manifest.Name) ? "Win32ThemeSystem" : manifest.Name;
                if (!string.IsNullOrWhiteSpace(manifest.AccentColor))
                    _resources["theme.accent"] = manifest.AccentColor;

                if (manifest.Resources != null)
                {
                    foreach ((string key, string value) in manifest.Resources)
                        _resources[key] = value;
                }
            }
            catch
            {
                _resources = BuildDefaults();
                _themeName = "Default";
            }
        }
    }

    private static Dictionary<string, string> BuildDefaults()
    {
        return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["theme.name"] = "Default",
            ["theme.accent"] = "#0078D4",
            ["color.surface"] = "#202124",
            ["color.surfaceAlt"] = "#2A2B2E",
            ["color.text"] = "#F5F7FA",
            ["font.family"] = "Segoe UI",
            ["font.size"] = "12",
        };
    }

    public Color GetAccentColor()
    {
        string hex = GetAccentColorHexOrDefault();
        try
        {
            return ColorTranslator.FromHtml(hex);
        }
        catch
        {
            return Color.FromArgb(0, 120, 212);
        }
    }
}
