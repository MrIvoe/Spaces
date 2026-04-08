#pragma once

#include <windows.h>

#include <string>
#include <memory>

class SettingsStore;
struct UniversalThemeData;

enum class ThemeMode
{
    Light,
    Dark
};

enum class ThemeStyle
{
    System,
    Win32ThemeCatalog
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

    // Space-specific tokens
    COLORREF spaceTitleBarColor = RGB(65, 65, 65);
    COLORREF spaceTitleTextColor = RGB(240, 240, 240);
    COLORREF spaceItemTextColor = RGB(200, 200, 200);
    COLORREF spaceItemHoverColor = RGB(85, 85, 85);
};

struct SpacePolicyDefaults
{
    bool rollupWhenNotHovered = false;
    bool transparentWhenNotHovered = false;
    bool labelsOnHover = true;
    std::wstring iconSpacingPreset = L"comfortable";
};

struct ThemeIconMapping
{
    std::wstring iconKey;
    std::wstring glyph;
    std::wstring assetPack;
    std::wstring assetName;
};

class ThemePlatform
{
public:
    explicit ThemePlatform(SettingsStore* store = nullptr);

    void SetStore(SettingsStore* store);

    ThemeMode ResolveMode() const;
    ThemeStyle ResolveStyle() const;
    int GetTextScalePercent() const;
    int GetSpaceIdleOpacityPercent() const;
    int GetSpaceTitleBarOpacityPercent() const;
    int GetSettingsWindowOpacityPercent() const;
    bool IsSettingsWindowBlurEnabled() const;
    int GetSettingsRowHeightPx() const;
    int GetSettingsRowGapPx() const;
    int GetSettingsSectionGapPx() const;
    int GetSettingsToggleWidthPx() const;
    int GetSettingsToggleHeightPx() const;
    int GetTrayMenuMinWidthPx() const;
    int GetTrayMenuRowHeightPx() const;
    SpacePolicyDefaults ResolveSpacePolicyDefaults() const;

    ThemePalette BuildPalette() const;
    ThemeIconMapping ResolveIconMapping(const std::wstring& iconKey,
                                        const std::wstring& fallbackGlyph = L"") const;
    bool ExportCustomPreset(const std::wstring& filePath) const;
    bool ImportCustomPreset(const std::wstring& filePath) const;

    static UINT GetThemeChangedMessageId();

private:
    static ThemeMode DetectSystemMode();
    static ThemePalette BuildPaletteFor(ThemeMode mode, ThemeStyle style);
    bool TryLoadUniversalTheme(std::shared_ptr<UniversalThemeData>& outTheme) const;

private:
    SettingsStore* m_store = nullptr;
    mutable std::shared_ptr<UniversalThemeData> m_cachedTheme;
};

