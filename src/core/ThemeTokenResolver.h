#pragma once

#include <string>
#include <unordered_map>
#include <windows.h>

class SettingsStore;
struct ThemePalette;

/// Resolves theme tokens to COLORREF values.
/// Maps token names (e.g., "win32.fence.border_color") to actual RGB colors.
class ThemeTokenResolver
{
public:
    explicit ThemeTokenResolver(SettingsStore* settingsStore);
    ~ThemeTokenResolver() = default;

    /// Resolve a token name to a COLORREF value.
    /// Falls back to default if token not found.
    COLORREF ResolveToken(const std::wstring& tokenName, COLORREF fallback) const;

    /// Resolve all known tokens to build a complete ThemePalette.
    ThemePalette BuildPaletteFromTokens(const std::unordered_map<std::wstring, std::wstring>& tokens) const;

    /// Convert hex color string (#RRGGBB) to COLORREF.
    static COLORREF HexToColorRef(const std::wstring& hexColor);

    /// Convert hex string (with or without #) to BGR COLORREF.
    static COLORREF HexToBGR(const std::string& hexStr);

    /// Get the canonical list of supported token names.
    static const std::unordered_map<std::wstring, COLORREF>& GetDefaultTokenMap();

private:
    SettingsStore* m_settingsStore = nullptr;

    /// Parse hex color string #RRGGBB to individual RGB components.
    static bool ParseHexColor(const std::wstring& hex, int& r, int& g, int& b);

    /// Build fallback palette when tokens are incomplete or unavailable.
    static ThemePalette GetFallbackPalette();
};
