#include "core/ThemeTokenResolver.h"

#include "core/SettingsStore.h"
#include "core/ThemePlatform.h"

#include <cctype>
#include <algorithm>
#include <cwctype>

namespace
{
    // Convert UTF-8 to wide string
    std::wstring Utf8ToWString(const std::string& s)
    {
        if (s.empty())
            return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (size <= 0)
            return {};
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
        return result;
    }

    // Default token color map (fallback values)
    const std::unordered_map<std::wstring, COLORREF> g_defaultTokens =
    {
        // Window/surface tokens
        {L"win32.base.window_color", RGB(255, 255, 255)},
        {L"win32.base.surface_color", RGB(245, 245, 245)},
        {L"win32.base.text_color", RGB(20, 20, 20)},
        {L"win32.base.subtle_text_color", RGB(90, 90, 90)},
        {L"win32.base.accent_color", RGB(70, 120, 220)},
        {L"win32.base.border_color", RGB(192, 192, 192)},

        // Space-specific tokens
        {L"win32.space.title_bar_color", RGB(65, 65, 65)},
        {L"win32.space.title_text_color", RGB(240, 240, 240)},
        {L"win32.space.item_text_color", RGB(200, 200, 200)},
        {L"win32.space.item_hover_color", RGB(85, 85, 85)},
        {L"win32.space.border_color", RGB(192, 192, 192)},
        {L"win32.space.item_selected_color", RGB(55, 55, 55)},
    };
}

ThemeTokenResolver::ThemeTokenResolver(SettingsStore* settingsStore)
    : m_settingsStore(settingsStore)
{
}

COLORREF ThemeTokenResolver::ResolveToken(const std::wstring& tokenName, COLORREF fallback) const
{
    // Normalize token name to lowercase
    std::wstring normalized = tokenName;
    for (auto& c : normalized)
    {
        if (iswupper(c))
            c = towlower(c);
    }

    // Check if it's in the default token map
    const auto& defaults = GetDefaultTokenMap();
    auto it = defaults.find(normalized);
    if (it != defaults.end())
        return it->second;

    // Fall back to provided default
    return fallback;
}

ThemePalette ThemeTokenResolver::BuildPaletteFromTokens(const std::unordered_map<std::wstring, std::wstring>& tokens) const
{
    ThemePalette palette = GetFallbackPalette();

    // Try to resolve each palette field from tokens
    auto resolveToken = [&tokens](const std::wstring& tokenName, COLORREF& outColor)
    {
        std::wstring normalized = tokenName;
        for (auto& c : normalized)
            if (iswupper(c))
                c = towlower(c);

        auto it = tokens.find(normalized);
        if (it != tokens.end())
        {
            COLORREF color = HexToColorRef(it->second);
            if (color != static_cast<COLORREF>(-1))
                outColor = color;
        }
    };

    // Map tokens to palette fields
    resolveToken(L"win32.base.window_color", palette.windowColor);
    resolveToken(L"win32.base.surface_color", palette.surfaceColor);
    resolveToken(L"win32.base.text_color", palette.textColor);
    resolveToken(L"win32.base.subtle_text_color", palette.subtleTextColor);
    resolveToken(L"win32.base.accent_color", palette.accentColor);
    resolveToken(L"win32.base.border_color", palette.borderColor);

    resolveToken(L"win32.space.title_bar_color", palette.spaceTitleBarColor);
    resolveToken(L"win32.space.title_text_color", palette.spaceTitleTextColor);
    resolveToken(L"win32.space.item_text_color", palette.spaceItemTextColor);
    resolveToken(L"win32.space.item_hover_color", palette.spaceItemHoverColor);

    return palette;
}

COLORREF ThemeTokenResolver::HexToColorRef(const std::wstring& hexColor)
{
    int r, g, b;
    if (ParseHexColor(hexColor, r, g, b))
    {
        // COLORREF uses BGR order (not RGB)
        return RGB(r, g, b);
    }
    return static_cast<COLORREF>(-1);  // Invalid
}

COLORREF ThemeTokenResolver::HexToBGR(const std::string& hexStr)
{
    return HexToColorRef(Utf8ToWString(hexStr));
}

const std::unordered_map<std::wstring, COLORREF>& ThemeTokenResolver::GetDefaultTokenMap()
{
    return g_defaultTokens;
}

bool ThemeTokenResolver::ParseHexColor(const std::wstring& hex, int& r, int& g, int& b)
{
    std::wstring normalized = hex;
    if (!normalized.empty() && normalized[0] == L'#')
        normalized = normalized.substr(1);

    if (normalized.length() != 6)
        return false;

    try
    {
        // Parse RRGGBB format
        r = std::stoi(normalized.substr(0, 2), nullptr, 16);
        g = std::stoi(normalized.substr(2, 2), nullptr, 16);
        b = std::stoi(normalized.substr(4, 2), nullptr, 16);

        return r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255;
    }
    catch (...)
    {
        return false;
    }
}

ThemePalette ThemeTokenResolver::GetFallbackPalette()
{
    return ThemePalette{
        RGB(255, 255, 255),  // windowColor
        RGB(245, 245, 245),  // surfaceColor
        RGB(245, 245, 245),  // navColor
        RGB(20, 20, 20),     // textColor
        RGB(90, 90, 90),     // subtleTextColor
        RGB(70, 120, 220),   // accentColor
        RGB(192, 192, 192),  // borderColor
        RGB(65, 65, 65),     // spaceTitleBarColor
        RGB(240, 240, 240),  // spaceTitleTextColor
        RGB(200, 200, 200),  // spaceItemTextColor
        RGB(85, 85, 85)      // spaceItemHoverColor
    };
}
