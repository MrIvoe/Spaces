using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace IVOEFences.Core.Services;

/// <summary>
/// Unified theme detection and application engine.
/// Detects Windows dark mode, high contrast, animations, transparency, and accent color.
/// </summary>
public sealed class ThemeEngine
{
    private static readonly Lazy<ThemeEngine> _instance = new(() => new ThemeEngine());
    public static ThemeEngine Instance => _instance.Value;

    // ── THEME MODE ──
    public enum ThemeMode
    {
        Auto,           // Follow system setting
        Light,          // Force light theme
        Dark,           // Force dark theme
        HighContrast    // Use high contrast mode
    }

    private ThemeMode _currentThemeMode = ThemeMode.Auto;

    // ── CACHED THEME STATE ──
    private bool _isDarkMode;
    private bool _isHighContrast;
    private bool _animationsEnabled;
    private bool _transparencyEnabled;
    private Color _accentColor;
    private DateTime _lastRefresh;
    private DateTime _lastAccentRefresh;
    private readonly object _refreshLock = new();

    private const int CACHE_DURATION_MS = 3000;
    private const int ACCENT_CACHE_DURATION_MS = 15000;

    public event EventHandler<ThemeChangedEventArgs>? ThemeChanged;

    private ThemeEngine()
    {
        RefreshAll();
    }

    /// <summary>
    /// Gets or sets the current theme mode (Auto, Light, Dark, HighContrast).
    /// Changing the mode refreshes cached state and fires ThemeChanged.
    /// </summary>
    public ThemeMode CurrentThemeMode
    {
        get => _currentThemeMode;
        set
        {
            if (_currentThemeMode != value)
            {
                _currentThemeMode = value;
                RefreshAll();
                ThemeChanged?.Invoke(this, new ThemeChangedEventArgs
                {
                    ThemeModeChanged = true,
                });
            }
        }
    }

    /// <summary>
    /// Gets or detects the current dark mode state.
    /// Respects CurrentThemeMode override.
    /// </summary>
    public bool IsDarkMode
    {
        get
        {
            if (_currentThemeMode == ThemeMode.Dark) return true;
            if (_currentThemeMode == ThemeMode.Light) return false;
            
            if (NeedsRefresh())
                RefreshAll();
            return _isDarkMode;
        }
    }

    /// <summary>
    /// Gets or detects the current high contrast state.
    /// </summary>
    public bool IsHighContrast
    {
        get
        {
            if (_currentThemeMode == ThemeMode.HighContrast) return true;
            if (NeedsRefresh())
                RefreshAll();
            return _isHighContrast;
        }
    }

    /// <summary>
    /// Gets or detects animations enabled state.
    /// </summary>
    public bool AnimationsEnabled
    {
        get
        {
            if (NeedsRefresh())
                RefreshAll();
            return _animationsEnabled;
        }
    }

    /// <summary>
    /// Gets or detects transparency enabled state.
    /// </summary>
    public bool TransparencyEnabled
    {
        get
        {
            if (NeedsRefresh())
                RefreshAll();
            return _transparencyEnabled;
        }
    }

    /// <summary>
    /// Gets or detects the current accent color.
    /// </summary>
    public Color AccentColor
    {
        get
        {
            if (NeedsRefresh())
                RefreshAll();
            return _accentColor;
        }
    }

    /// <summary>
    /// Animation duration based on settings (200ms if enabled, 0 if disabled).
    /// </summary>
    public TimeSpan AnimationDuration => AnimationsEnabled ? TimeSpan.FromMilliseconds(200) : TimeSpan.Zero;

    // ── DETECTION METHODS ──

    public bool DetectDarkMode()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
            return (int)(key?.GetValue("AppsUseLightTheme") ?? 1) == 0;
        }
        catch
        {
            return false;
        }
    }

    public bool DetectHighContrast()
    {
        try
        {
            var hc = new HIGHCONTRAST { cbSize = Marshal.SizeOf<HIGHCONTRAST>() };
            SystemParametersInfo(0x0042 /*SPI_GETHIGHCONTRAST*/, hc.cbSize, ref hc, 0);
            return (hc.dwFlags & 0x00000001 /*HCF_HIGHCONTRASTON*/) != 0;
        }
        catch
        {
            return false;
        }
    }

    public bool DetectAnimations()
    {
        try
        {
            bool enabled = true;
            SystemParametersInfo(0x1012 /*SPI_GETCLIENTAREAANIMATION*/, 0, ref enabled, 0);
            return enabled;
        }
        catch
        {
            return true;
        }
    }

    public bool DetectTransparency()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
            return (int)(key?.GetValue("EnableTransparency") ?? 1) != 0;
        }
        catch
        {
            return true;
        }
    }

    public Color DetectAccentColor()
    {
        try
        {
            DwmGetColorizationColor(out uint argb, out bool _);
            byte r = (byte)((argb >> 16) & 0xFF);
            byte g = (byte)((argb >> 8) & 0xFF);
            byte b = (byte)(argb & 0xFF);
            return Color.FromArgb(r, g, b);
        }
        catch
        {
            return Color.FromArgb(0, 120, 215); // Windows 11 default blue
        }
    }

    // ── PUBLIC REFRESH ──

    /// <summary>
    /// Force re-detection of all theme properties.
    /// Call this on WM_SETTINGCHANGE or after user changes preferences.
    /// </summary>
    public void RefreshAll()
    {
        lock (_refreshLock)
        {
            var oldDarkMode = _isDarkMode;
            var oldHighContrast = _isHighContrast;
            var oldAnimations = _animationsEnabled;
            var oldTransparency = _transparencyEnabled;
            var oldAccent = _accentColor;

            _isDarkMode = DetectDarkMode();
            _isHighContrast = DetectHighContrast();
            _animationsEnabled = DetectAnimations();
            _transparencyEnabled = DetectTransparency();

            DateTime now = DateTime.UtcNow;
            bool shouldRefreshAccent =
                _accentColor == default
                || (now - _lastAccentRefresh).TotalMilliseconds > ACCENT_CACHE_DURATION_MS;
            if (shouldRefreshAccent)
            {
                _accentColor = DetectAccentColor();
                _lastAccentRefresh = now;
            }

            _lastRefresh = now;

            // Notify if anything changed
            if (oldDarkMode != _isDarkMode ||
                oldHighContrast != _isHighContrast ||
                oldAnimations != _animationsEnabled ||
                oldTransparency != _transparencyEnabled ||
                !oldAccent.Equals(_accentColor))
            {
                ThemeChanged?.Invoke(this, new ThemeChangedEventArgs
                {
                    DarkModeChanged = oldDarkMode != _isDarkMode,
                    HighContrastChanged = oldHighContrast != _isHighContrast,
                    AnimationsChanged = oldAnimations != _animationsEnabled,
                    TransparencyChanged = oldTransparency != _transparencyEnabled,
                    AccentChanged = !oldAccent.Equals(_accentColor)
                });
            }
        }
    }

    /// <summary>
    /// Checks if cache has expired (> 5 seconds old).
    /// </summary>
    private bool NeedsRefresh()
    {
        return (DateTime.UtcNow - _lastRefresh).TotalMilliseconds > CACHE_DURATION_MS;
    }

    // ── UTILITY METHODS ──

    /// <summary>
    /// Determines if a color is perceptually dark.
    /// Uses luminance formula: 0.2126*R + 0.7152*G + 0.0722*B
    /// </summary>
    public bool IsColorDark(Color c)
    {
        double luminance = 0.2126 * c.R / 255.0 + 0.7152 * c.G / 255.0 + 0.0722 * c.B / 255.0;
        return luminance < 0.5;
    }

    /// <summary>
    /// Gets the text foreground color (white or black) based on background.
    /// </summary>
    public Color GetForegroundFor(Color background)
    {
        return IsColorDark(background) ? Color.White : Color.Black;
    }

    // ── P/INVOKE & STRUCTURES ──

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SystemParametersInfo(int uiAction, int uiParam, ref bool pvParam, int fWinIni);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SystemParametersInfo(int uiAction, int uiParam, ref HIGHCONTRAST pvParam, int fWinIni);

    [DllImport("dwmapi.dll")]
    private static extern int DwmGetColorizationColor(out uint pcrColorization, out bool pfOpaqueBlend);

    [StructLayout(LayoutKind.Sequential)]
    private struct HIGHCONTRAST
    {
        public int cbSize;
        public int dwFlags;
        public IntPtr lpszDefaultScheme;
    }

    // ── EVENT ARGS ──

    public class ThemeChangedEventArgs : EventArgs
    {
        public bool DarkModeChanged { get; set; }
        public bool HighContrastChanged { get; set; }
        public bool AnimationsChanged { get; set; }
        public bool TransparencyChanged { get; set; }
        public bool AccentChanged { get; set; }
        public bool ThemeModeChanged { get; set; }
    }

    // ── TYPED PALETTE ──

    /// <summary>
    /// Strongly-typed color palette for a theme.
    /// </summary>
    public sealed record ThemePalette
    {
        // Base colors
        public Color WindowBackground { get; init; } = Color.FromArgb(255, 255, 255);
        public Color WindowForeground { get; init; } = Color.FromArgb(0, 0, 0);
        public Color PanelBackground { get; init; } = Color.FromArgb(243, 243, 243);
        public Color PanelBorder { get; init; } = Color.FromArgb(233, 233, 233);

        // Text
        public Color TextPrimary { get; init; } = Color.FromArgb(0, 0, 0);
        public Color TextSecondary { get; init; } = Color.FromArgb(100, 100, 100);
        public Color TextDisabled { get; init; } = Color.FromArgb(184, 184, 184);

        // Buttons
        public Color ButtonBackground { get; init; } = Color.FromArgb(231, 231, 231);
        public Color ButtonForeground { get; init; } = Color.FromArgb(0, 0, 0);
        public Color ButtonHoverBackground { get; init; } = Color.FromArgb(211, 211, 211);
        public Color AccentButtonBackground { get; init; } = Color.FromArgb(0, 120, 212);
        public Color AccentButtonForeground { get; init; } = Color.FromArgb(255, 255, 255);

        // Controls
        public Color TabControlBackground { get; init; } = Color.FromArgb(255, 255, 255);
        public Color TabControlForeground { get; init; } = Color.FromArgb(0, 0, 0);
        public Color TabItemBackground { get; init; } = Color.FromArgb(243, 243, 243);
        public Color TabItemSelectedBackground { get; init; } = Color.FromArgb(255, 255, 255);

        // Fences
        public Color FenceBackground { get; init; } = Color.FromArgb(232, 232, 232);
        public Color FenceTitleBackground { get; init; } = Color.FromArgb(0, 120, 212);
        public Color FenceTitleForeground { get; init; } = Color.FromArgb(255, 255, 255);
        public Color FenceBorder { get; init; } = Color.FromArgb(197, 197, 197);
    }

    // ── THEME APPLICATION ──

    /// <summary>
    /// Get the effective theme mode (merged system + user preference).
    /// </summary>
    public ThemeMode GetEffectiveThemeMode()
    {
        if (_currentThemeMode != ThemeMode.Auto)
            return _currentThemeMode;

        if (IsHighContrast)
            return ThemeMode.HighContrast;

        return IsDarkMode ? ThemeMode.Dark : ThemeMode.Light;
    }

    /// <summary>
    /// Get the typed color palette for the current effective theme.
    /// </summary>
    public ThemePalette GetPalette()
    {
        var effectiveMode = GetEffectiveThemeMode();
        
        return effectiveMode switch
        {
            ThemeMode.Dark => DarkPalette,
            ThemeMode.HighContrast => HighContrastPalette,
            _ => LightPalette
        };
    }

    /// <summary>
    /// Get color palette as a string dictionary (legacy compatibility).
    /// </summary>
    public Dictionary<string, string> GetColorPalette()
    {
        var p = GetPalette();
        return new Dictionary<string, string>
        {
            { "WindowBackground", ToHex(p.WindowBackground) },
            { "WindowForeground", ToHex(p.WindowForeground) },
            { "PanelBackground", ToHex(p.PanelBackground) },
            { "PanelBorder", ToHex(p.PanelBorder) },
            { "TextPrimary", ToHex(p.TextPrimary) },
            { "TextSecondary", ToHex(p.TextSecondary) },
            { "TextDisabled", ToHex(p.TextDisabled) },
            { "ButtonBackground", ToHex(p.ButtonBackground) },
            { "ButtonForeground", ToHex(p.ButtonForeground) },
            { "ButtonHoverBackground", ToHex(p.ButtonHoverBackground) },
            { "AccentButtonBackground", ToHex(p.AccentButtonBackground) },
            { "AccentButtonForeground", ToHex(p.AccentButtonForeground) },
            { "TabControlBackground", ToHex(p.TabControlBackground) },
            { "TabControlForeground", ToHex(p.TabControlForeground) },
            { "TabItemBackground", ToHex(p.TabItemBackground) },
            { "TabItemSelectedBackground", ToHex(p.TabItemSelectedBackground) },
            { "FenceBackground", ToHex(p.FenceBackground) },
            { "FenceTitleBackground", ToHex(p.FenceTitleBackground) },
            { "FenceTitleForeground", ToHex(p.FenceTitleForeground) },
            { "FenceBorder", ToHex(p.FenceBorder) },
        };
    }

    private static readonly ThemePalette LightPalette = new();

    private static readonly ThemePalette DarkPalette = new()
    {
        WindowBackground = Color.FromArgb(30, 30, 30),
        WindowForeground = Color.FromArgb(255, 255, 255),
        PanelBackground = Color.FromArgb(45, 45, 45),
        PanelBorder = Color.FromArgb(64, 64, 64),
        TextPrimary = Color.FromArgb(255, 255, 255),
        TextSecondary = Color.FromArgb(176, 176, 176),
        TextDisabled = Color.FromArgb(96, 96, 96),
        ButtonBackground = Color.FromArgb(60, 60, 60),
        ButtonForeground = Color.FromArgb(255, 255, 255),
        ButtonHoverBackground = Color.FromArgb(74, 74, 74),
        AccentButtonBackground = Color.FromArgb(0, 120, 212),
        AccentButtonForeground = Color.FromArgb(255, 255, 255),
        TabControlBackground = Color.FromArgb(30, 30, 30),
        TabControlForeground = Color.FromArgb(255, 255, 255),
        TabItemBackground = Color.FromArgb(45, 45, 45),
        TabItemSelectedBackground = Color.FromArgb(60, 60, 60),
        FenceBackground = Color.FromArgb(45, 45, 45),
        FenceTitleBackground = Color.FromArgb(0, 120, 212),
        FenceTitleForeground = Color.FromArgb(255, 255, 255),
        FenceBorder = Color.FromArgb(64, 64, 64),
    };

    private static readonly ThemePalette HighContrastPalette = new()
    {
        WindowBackground = Color.FromArgb(0, 0, 0),
        WindowForeground = Color.FromArgb(255, 255, 255),
        PanelBackground = Color.FromArgb(0, 0, 0),
        PanelBorder = Color.FromArgb(255, 255, 255),
        TextPrimary = Color.FromArgb(255, 255, 255),
        TextSecondary = Color.FromArgb(255, 255, 255),
        TextDisabled = Color.FromArgb(128, 128, 128),
        ButtonBackground = Color.FromArgb(0, 0, 0),
        ButtonForeground = Color.FromArgb(255, 255, 255),
        ButtonHoverBackground = Color.FromArgb(64, 64, 64),
        AccentButtonBackground = Color.FromArgb(255, 255, 0),
        AccentButtonForeground = Color.FromArgb(0, 0, 0),
        TabControlBackground = Color.FromArgb(0, 0, 0),
        TabControlForeground = Color.FromArgb(255, 255, 255),
        TabItemBackground = Color.FromArgb(0, 0, 0),
        TabItemSelectedBackground = Color.FromArgb(255, 255, 255),
        FenceBackground = Color.FromArgb(0, 0, 0),
        FenceTitleBackground = Color.FromArgb(255, 255, 0),
        FenceTitleForeground = Color.FromArgb(0, 0, 0),
        FenceBorder = Color.FromArgb(255, 255, 255),
    };

    private static string ToHex(Color color)
    {
        return $"#{color.R:X2}{color.G:X2}{color.B:X2}";
    }

    /// <summary>
    /// Get system accent color as hex string.
    /// </summary>
    public string GetAccentColorHex()
    {
        var accent = AccentColor;
        return $"#{accent.R:X2}{accent.G:X2}{accent.B:X2}";
    }

    /// <summary>
    /// Force theme application (e.g., when user changes theme in Settings).
    /// </summary>
    public void ApplyTheme(ThemeMode mode)
    {
        CurrentThemeMode = mode;
    }
}

