#pragma once

#include <windows.h>

#include <string>

class SettingsStore;

enum class ThemeMode
{
    Light,
    Dark
};

enum class ThemeStyle
{
    System,
    Discord,
    Fences,
    GitHubDark,
    GitHubDarkDimmed,
    GitHubLight,
    Win32ThemeCatalog,
    Custom
};

// Shared palette tokens for all UI surfaces.
struct ThemePalette
{
    COLORREF windowColor = RGB(255, 255, 255);
    COLORREF surfaceColor = RGB(255, 255, 255);
    COLORREF navColor = RGB(245, 245, 245);
    COLORREF textColor = RGB(20, 20, 20);
    COLORREF subtleTextColor = RGB(90, 90, 90);
    COLORREF accentColor = RGB(70, 120, 220);
    COLORREF borderColor = RGB(192, 192, 192);

    // Fence-specific tokens
    COLORREF fenceTitleBarColor = RGB(65, 65, 65);
    COLORREF fenceTitleTextColor = RGB(240, 240, 240);
    COLORREF fenceItemTextColor = RGB(200, 200, 200);
    COLORREF fenceItemHoverColor = RGB(85, 85, 85);
};

struct FencePolicyDefaults
{
    bool rollupWhenNotHovered = false;
    bool transparentWhenNotHovered = false;
    bool labelsOnHover = true;
    std::wstring iconSpacingPreset = L"comfortable";
};

class ThemePlatform
{
public:
    explicit ThemePlatform(SettingsStore* store = nullptr);

    void SetStore(SettingsStore* store);

    ThemeMode ResolveMode() const;
    ThemeStyle ResolveStyle() const;
    int GetTextScalePercent() const;
    FencePolicyDefaults ResolveFencePolicyDefaults() const;

    ThemePalette BuildPalette() const;
    bool ExportCustomPreset(const std::wstring& filePath) const;
    bool ImportCustomPreset(const std::wstring& filePath) const;

    static UINT GetThemeChangedMessageId();

private:
    static ThemeMode DetectSystemMode();
    static ThemePalette BuildPaletteFor(ThemeMode mode, ThemeStyle style);

private:
    SettingsStore* m_store = nullptr;
};
