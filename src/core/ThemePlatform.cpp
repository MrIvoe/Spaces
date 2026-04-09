#include "core/ThemePlatform.h"

#include "core/SettingsStore.h"
#include "core/UniversalThemeLoader.h"

#include <nlohmann/json.hpp>

#include <cwctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

#include <dwmapi.h>
#include <windows.h>

namespace
{
    std::string WStringToUtf8(const std::wstring& ws)
    {
        if (ws.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWString(const std::string& s)
    {
        if (s.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (size <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
        return result;
    }

    std::wstring ColorToHex(COLORREF color)
    {
        wchar_t buffer[8]{};
        swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
        return buffer;
    }

    std::optional<COLORREF> ParseHexColor(const std::wstring& raw)
    {
        std::wstring text;
        text.reserve(raw.size());
        for (wchar_t c : raw)
        {
            if (!iswspace(c))
            {
                text.push_back(c);
            }
        }

        if (text.empty())
        {
            return std::nullopt;
        }
        if (text[0] == L'#')
        {
            text.erase(text.begin());
        }
        if (text.size() != 6)
        {
            return std::nullopt;
        }

        auto hexToInt = [](wchar_t c) -> int {
            if (c >= L'0' && c <= L'9') return c - L'0';
            if (c >= L'a' && c <= L'f') return 10 + (c - L'a');
            if (c >= L'A' && c <= L'F') return 10 + (c - L'A');
            return -1;
        };

        int bytes[3]{};
        for (int i = 0; i < 3; ++i)
        {
            const int hi = hexToInt(text[i * 2]);
            const int lo = hexToInt(text[i * 2 + 1]);
            if (hi < 0 || lo < 0)
            {
                return std::nullopt;
            }
            bytes[i] = (hi << 4) | lo;
        }

        return RGB(bytes[0], bytes[1], bytes[2]);
    }

    COLORREF BlendColor(COLORREF from, COLORREF to, int alpha)
    {
        alpha = (alpha < 0) ? 0 : ((alpha > 255) ? 255 : alpha);
        const int inv = 255 - alpha;
        const BYTE red = static_cast<BYTE>(((GetRValue(from) * inv) + (GetRValue(to) * alpha)) / 255);
        const BYTE green = static_cast<BYTE>(((GetGValue(from) * inv) + (GetGValue(to) * alpha)) / 255);
        const BYTE blue = static_cast<BYTE>(((GetBValue(from) * inv) + (GetBValue(to) * alpha)) / 255);
        return RGB(red, green, blue);
    }

    std::optional<COLORREF> DetectSystemAccentColor()
    {
        DWORD colorization = 0;
        BOOL opaqueBlend = FALSE;
        if (SUCCEEDED(DwmGetColorizationColor(&colorization, &opaqueBlend)))
        {
            const BYTE red = static_cast<BYTE>((colorization >> 16) & 0xFF);
            const BYTE green = static_cast<BYTE>((colorization >> 8) & 0xFF);
            const BYTE blue = static_cast<BYTE>(colorization & 0xFF);
            return RGB(red, green, blue);
        }

        DWORD accent = 0;
        DWORD accentSize = sizeof(accent);
        const LSTATUS status = RegGetValueW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\DWM",
            L"ColorizationColor",
            RRF_RT_REG_DWORD,
            nullptr,
            &accent,
            &accentSize);
        if (status != ERROR_SUCCESS)
        {
            return std::nullopt;
        }

        const BYTE red = static_cast<BYTE>((accent >> 16) & 0xFF);
        const BYTE green = static_cast<BYTE>((accent >> 8) & 0xFF);
        const BYTE blue = static_cast<BYTE>(accent & 0xFF);
        return RGB(red, green, blue);
    }

    bool IsWin32ThemeDark(const std::wstring& themeId)
    {
        return themeId == L"amber-terminal" ||
               themeId == L"arctic-glass" ||
               themeId == L"graphite-office" ||
               themeId == L"neon-cyberpunk" ||
               themeId == L"nocturne-dark" ||
               themeId == L"storm-steel" ||
               themeId == L"harbor-blue" ||
               themeId == L"olive-terminal";
    }

    const std::unordered_map<std::wstring, std::wstring>& BuiltinFallbackGlyphs()
    {
        static const std::unordered_map<std::wstring, std::wstring> kMap = {
            {L"settings.overview", L"\uE80F"},
            {L"settings.general", L"\uE713"},
            {L"settings.appearance", L"\uE790"},
            {L"settings.plugins", L"\uE943"},
            {L"settings.tray.behavior", L"\uEA8F"},
            {L"plugins.builtin.settings", L"\uE713"},
            {L"plugins.builtin.tray", L"\uEA8F"},
            {L"plugins.builtin.widgets", L"\uE9CA"},
            {L"plugins.builtin.explorer_portal", L"\uE8B7"},
            {L"plugins.builtin.core_commands", L"\uE943"},
            {L"plugins.generic", L"\uE943"},
            {L"actions.space.create", L"\uE710"},
            {L"actions.space.rename", L"\uE70F"},
            {L"actions.space.delete", L"\uE74D"},
            {L"actions.item.open", L"\uE8A7"},
            {L"actions.item.delete", L"\uE74D"},
            {L"actions.plugin.openSettings", L"\uE713"},
            {L"actions.app.exit", L"\uE7E8"},
        };
        return kMap;
    }

    const std::unordered_map<std::wstring, std::wstring>& BuiltinTablerAssets()
    {
        static const std::unordered_map<std::wstring, std::wstring> kMap = {
            {L"settings.overview", L"layout-dashboard"},
            {L"settings.general", L"settings"},
            {L"settings.appearance", L"palette"},
            {L"settings.plugins", L"puzzle"},
            {L"settings.tray.behavior", L"bell"},
            {L"plugins.builtin.settings", L"settings"},
            {L"plugins.builtin.tray", L"bell"},
            {L"plugins.builtin.widgets", L"layout-cards"},
            {L"plugins.builtin.explorer_portal", L"folder"},
            {L"plugins.builtin.core_commands", L"command"},
            {L"actions.space.create", L"plus"},
            {L"actions.space.rename", L"edit"},
            {L"actions.space.delete", L"trash"},
            {L"actions.item.open", L"external-link"},
            {L"actions.item.delete", L"trash-x"},
            {L"actions.plugin.openSettings", L"settings"},
            {L"actions.app.exit", L"power"},
        };
        return kMap;
    }

    ThemePalette BuildWin32ThemePalette(const std::wstring& themeId, const std::optional<ThemeMode>& forcedMode)
    {
        const std::wstring key = themeId.empty() ? L"graphite-office" : themeId;

        bool dark = IsWin32ThemeDark(key);
        if (forcedMode.has_value())
        {
            dark = (*forcedMode == ThemeMode::Dark);
        }

        ThemePalette palette;
        if (dark)
        {
            palette.windowColor = RGB(34, 39, 46);
            palette.surfaceColor = RGB(44, 50, 58);
            palette.navColor = RGB(28, 33, 40);
            palette.textColor = RGB(173, 186, 199);
            palette.subtleTextColor = RGB(118, 131, 144);
            palette.accentColor = RGB(83, 155, 245);
            palette.borderColor = RGB(68, 76, 86);
            palette.spaceTitleBarColor = RGB(39, 45, 53);
            palette.spaceTitleTextColor = RGB(199, 210, 223);
            palette.spaceItemTextColor = RGB(173, 186, 199);
            palette.spaceItemHoverColor = RGB(55, 63, 72);
        }
        else
        {
            palette.windowColor = RGB(246, 248, 250);
            palette.surfaceColor = RGB(255, 255, 255);
            palette.navColor = RGB(242, 245, 248);
            palette.textColor = RGB(31, 35, 40);
            palette.subtleTextColor = RGB(87, 96, 106);
            palette.accentColor = RGB(9, 105, 218);
            palette.borderColor = RGB(208, 215, 222);
            palette.spaceTitleBarColor = RGB(234, 238, 242);
            palette.spaceTitleTextColor = RGB(31, 35, 40);
            palette.spaceItemTextColor = RGB(36, 41, 47);
            palette.spaceItemHoverColor = RGB(230, 236, 241);
        }

        if (key == L"graphite-office") palette.accentColor = RGB(90, 113, 143);
        else if (key == L"amber-terminal") palette.accentColor = RGB(242, 163, 68);
        else if (key == L"arctic-glass") palette.accentColor = RGB(123, 189, 221);
        else if (key == L"aurora-light") palette.accentColor = RGB(86, 150, 243);
        else if (key == L"brass-steampunk") palette.accentColor = RGB(166, 122, 71);
        else if (key == L"copper-foundry") palette.accentColor = RGB(181, 104, 76);
        else if (key == L"emerald-ledger") palette.accentColor = RGB(55, 158, 117);
        else if (key == L"forest-organic") palette.accentColor = RGB(74, 146, 92);
        else if (key == L"harbor-blue") palette.accentColor = RGB(52, 126, 184);
        else if (key == L"ivory-bureau") palette.accentColor = RGB(147, 128, 94);
        else if (key == L"mono-minimal") palette.accentColor = RGB(120, 124, 129);
        else if (key == L"neon-cyberpunk") palette.accentColor = RGB(235, 63, 255);
        else if (key == L"nocturne-dark") palette.accentColor = RGB(109, 96, 178);
        else if (key == L"nova-futuristic") palette.accentColor = RGB(86, 190, 255);
        else if (key == L"olive-terminal") palette.accentColor = RGB(146, 170, 83);
        else if (key == L"pop-colorburst") palette.accentColor = RGB(243, 101, 139);
        else if (key == L"rose-paper") palette.accentColor = RGB(212, 122, 148);
        else if (key == L"storm-steel") palette.accentColor = RGB(94, 124, 152);
        else if (key == L"sunset-retro") palette.accentColor = RGB(236, 129, 84);
        else if (key == L"tape-lo-fi") palette.accentColor = RGB(121, 112, 137);

        palette.spaceTitleBarColor = BlendColor(palette.spaceTitleBarColor, palette.accentColor, dark ? 120 : 90);
        palette.borderColor = BlendColor(palette.borderColor, palette.accentColor, 72);
        palette.spaceItemHoverColor = BlendColor(palette.spaceItemHoverColor, palette.accentColor, dark ? 55 : 40);
        return palette;
    }

    std::wstring GlyphForAssetName(const std::wstring& assetName)
    {
        static const std::unordered_map<std::wstring, std::wstring> kAssetGlyphs = {
            {L"layout-dashboard", L"\uE80F"},
            {L"settings", L"\uE713"},
            {L"palette", L"\uE790"},
            {L"puzzle", L"\uE943"},
            {L"bell", L"\uEA8F"},
            {L"layout-cards", L"\uF0E2"},
            {L"folder", L"\uE8B7"},
            {L"command", L"\uE943"},
            {L"plus", L"\uE710"},
            {L"edit", L"\uE70F"},
            {L"trash", L"\uE74D"},
            {L"trash-x", L"\uE74D"},
            {L"external-link", L"\uE8A7"},
            {L"power", L"\uE7E8"},
        };

        const auto it = kAssetGlyphs.find(assetName);
        return (it != kAssetGlyphs.end()) ? it->second : L"";
    }

    std::optional<std::string> FindResolvedValue(const std::unordered_map<std::string, std::string>& map,
                                                 const std::string& key)
    {
        const auto it = map.find(key);
        if (it == map.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

    std::optional<int> ParseThemeInt(const std::string& raw)
    {
        try
        {
            size_t consumed = 0;
            const double value = std::stod(raw, &consumed);
            if (consumed == 0)
            {
                return std::nullopt;
            }

            return static_cast<int>(std::lround(value));
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<COLORREF> ResolveComponentColor(const UniversalThemeData& theme, const std::string& componentPath)
    {
        const std::optional<std::string> raw = FindResolvedValue(theme.components, componentPath);
        if (!raw.has_value())
        {
            return std::nullopt;
        }

        return ParseHexColor(Utf8ToWString(*raw));
    }

    std::optional<int> ResolveComponentInt(const UniversalThemeData& theme, const std::string& componentPath)
    {
        const std::optional<std::string> raw = FindResolvedValue(theme.components, componentPath);
        if (!raw.has_value())
        {
            return std::nullopt;
        }

        return ParseThemeInt(*raw);
    }

    int ResolveThemeMetric(const SettingsStore* store,
                           const UniversalThemeData* theme,
                           const wchar_t* storeKey,
                           const char* componentPath,
                           int fallback,
                           int minValue,
                           int maxValue)
    {
        int value = fallback;
        if (theme && componentPath)
        {
            const std::optional<int> themed = ResolveComponentInt(*theme, componentPath);
            if (themed.has_value())
            {
                value = *themed;
            }
        }

        if (store)
        {
            try
            {
                value = std::stoi(store->Get(storeKey, std::to_wstring(value)));
            }
            catch (...)
            {
            }
        }

        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
        return value;
    }
}

ThemePlatform::ThemePlatform(SettingsStore* store)
    : m_store(store)
{
}

void ThemePlatform::SetStore(SettingsStore* store)
{
    m_store = store;
    m_cachedTheme.reset();
}

ThemeMode ThemePlatform::DetectSystemMode()
{
    DWORD lightThemeEnabled = 1;
    DWORD valueSize = sizeof(lightThemeEnabled);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &lightThemeEnabled,
        &valueSize);

    if (status != ERROR_SUCCESS)
    {
        return ThemeMode::Light;
    }

    return (lightThemeEnabled == 0) ? ThemeMode::Dark : ThemeMode::Light;
}

ThemeMode ThemePlatform::ResolveMode() const
{
    if (!m_store)
    {
        return DetectSystemMode();
    }

    const std::wstring modeValue = m_store->Get(L"appearance.theme.mode", L"system");
    if (modeValue == L"dark")
    {
        return ThemeMode::Dark;
    }
    if (modeValue == L"light")
    {
        return ThemeMode::Light;
    }

    // "Follow system" in Win32ThemeSystem mode follows each preset's
    // intended light/dark profile instead of forcing the OS preference.
    if (m_store->Get(L"theme.source", L"") == L"win32_theme_system")
    {
        const std::wstring themeId = m_store->Get(L"theme.win32.theme_id", L"graphite-office");
        return IsWin32ThemeDark(themeId) ? ThemeMode::Dark : ThemeMode::Light;
    }

    return DetectSystemMode();
}

ThemeStyle ThemePlatform::ResolveStyle() const
{
    (void)m_store;
    return ThemeStyle::Win32ThemeCatalog;
}

int ThemePlatform::GetTextScalePercent() const
{
    int textScale = 115;
    if (m_store)
    {
        std::wstring raw = m_store->Get(L"appearance.text.scale_percent", L"");
        if (raw.empty())
        {
            raw = m_store->Get(L"appearance.theme.text_scale_percent", L"115");
        }
        try
        {
            textScale = std::stoi(raw);
        }
        catch (...)
        {
            textScale = 115;
        }
    }

    if (textScale < 90)
    {
        textScale = 90;
    }
    if (textScale > 150)
    {
        textScale = 150;
    }

    return textScale;
}

int ThemePlatform::GetSpaceIdleOpacityPercent() const
{
    int value = 92;
    if (m_store)
    {
        try
        {
            value = std::stoi(m_store->Get(L"appearance.ui.space_idle_opacity_percent", L"92"));
        }
        catch (...)
        {
            value = 92;
        }
    }

    if (value < 5) value = 5;
    if (value > 100) value = 100;
    return value;
}

int ThemePlatform::GetSpaceTitleBarOpacityPercent() const
{
    int value = 96;
    if (m_store)
    {
        try
        {
            value = std::stoi(m_store->Get(L"appearance.ui.space_titlebar_opacity_percent", L"96"));
        }
        catch (...)
        {
            value = 96;
        }
    }

    if (value < 5) value = 5;
    if (value > 100) value = 100;
    return value;
}

int ThemePlatform::GetSettingsWindowOpacityPercent() const
{
    int value = 100;
    if (m_store)
    {
        try
        {
            value = std::stoi(m_store->Get(L"appearance.ui.settings_window_opacity_percent", L"100"));
        }
        catch (...)
        {
            value = 100;
        }
    }

    if (value < 85) value = 85;
    if (value > 100) value = 100;
    return value;
}

bool ThemePlatform::IsSettingsWindowBlurEnabled() const
{
    if (!m_store)
    {
        return false;
    }

    return m_store->Get(L"appearance.ui.settings_window_blur_enabled", L"false") == L"true";
}

int ThemePlatform::GetSettingsRowHeightPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.settings_row_height_px",
                              "settings.default.rowHeight",
                              34,
                              20,
                              72);
}

int ThemePlatform::GetSettingsRowGapPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.settings_row_gap_px",
                              "settings.default.rowGap",
                              6,
                              0,
                              32);
}

int ThemePlatform::GetSettingsSectionGapPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.settings_section_gap_px",
                              "settings.default.sectionGap",
                              22,
                              0,
                              72);
}

int ThemePlatform::GetSettingsToggleWidthPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.settings_toggle_width_px",
                              "toggle.default.trackWidth",
                              62,
                              36,
                              220);
}

int ThemePlatform::GetSettingsToggleHeightPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.settings_toggle_height_px",
                              "toggle.default.trackHeight",
                              28,
                              14,
                              64);
}

int ThemePlatform::GetTrayMenuMinWidthPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.tray_menu_min_width_px",
                              "tray.default.minWidth",
                              220,
                              140,
                              520);
}

int ThemePlatform::GetTrayMenuRowHeightPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.tray_menu_row_height_px",
                              "tray.default.itemHeight",
                              28,
                              18,
                              72);
}

int ThemePlatform::GetFenceTitleBarHeightPx() const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    return ResolveThemeMetric(m_store,
                              TryLoadUniversalTheme(universalTheme) ? universalTheme.get() : nullptr,
                              L"appearance.ui.space_titlebar_height_px",
                              "fence.default.titlebarHeight",
                              28,
                              20,
                              72);
}

int ThemePlatform::GetMotionDurationMs(const std::wstring& motionKey, int fallbackMs) const
{
    std::shared_ptr<UniversalThemeData> universalTheme;
    if (TryLoadUniversalTheme(universalTheme))
    {
        const std::string key = WStringToUtf8(motionKey);
        const auto it = universalTheme->motion.find(key);
        if (it != universalTheme->motion.end())
        {
            const std::optional<int> resolved = ParseThemeInt(it->second);
            if (resolved.has_value())
            {
                return *resolved;
            }
        }
    }

    return fallbackMs;
}

SpacePolicyDefaults ThemePlatform::ResolveSpacePolicyDefaults() const
{
    SpacePolicyDefaults defaults;
    defaults.rollupWhenNotHovered = false;
    defaults.transparentWhenNotHovered = false;
    defaults.labelsOnHover = true;
    defaults.iconSpacingPreset = L"comfortable";
    return defaults;
}

ThemePalette ThemePlatform::BuildPaletteFor(ThemeMode mode, ThemeStyle style)
{
    ThemePalette palette;

    if (mode == ThemeMode::Dark)
    {
        palette.windowColor = RGB(24, 24, 24);
        palette.surfaceColor = RGB(36, 36, 36);
        palette.navColor = RGB(30, 30, 30);
        palette.textColor = RGB(232, 232, 232);
        palette.subtleTextColor = RGB(180, 180, 180);
        palette.accentColor = RGB(88, 101, 242);
        palette.borderColor = RGB(72, 72, 72);

        palette.spaceTitleBarColor = RGB(58, 58, 58);
        palette.spaceTitleTextColor = RGB(239, 239, 239);
        palette.spaceItemTextColor = RGB(210, 210, 210);
        palette.spaceItemHoverColor = RGB(82, 82, 82);
    }
    else
    {
        palette.windowColor = RGB(248, 249, 252);
        palette.surfaceColor = RGB(255, 255, 255);
        palette.navColor = RGB(240, 242, 246);
        palette.textColor = RGB(32, 34, 37);
        palette.subtleTextColor = RGB(95, 101, 112);
        palette.accentColor = RGB(78, 91, 242);
        palette.borderColor = RGB(205, 210, 218);

        palette.spaceTitleBarColor = RGB(216, 226, 235);
        palette.spaceTitleTextColor = RGB(30, 42, 54);
        palette.spaceItemTextColor = RGB(43, 58, 72);
        palette.spaceItemHoverColor = RGB(201, 219, 235);
    }

    (void)style;
    return palette;
}

bool ThemePlatform::TryLoadUniversalTheme(std::shared_ptr<UniversalThemeData>& outTheme) const
{
    // First check if we have a cached theme
    if (m_cachedTheme)
    {
        outTheme = m_cachedTheme;
        return true;
    }

    if (!m_store)
    {
        return false;
    }

    // Try to determine the theme directory
    // For now, check common location: %APPDATA%/SimpleSpaces/Spaces/themes/
    // In a full implementation, this would be configurable
    std::wstring themePath = m_store->Get(L"theme.custom.path", L"");
    if (themePath.empty())
    {
        // Default to user's Custom theme folder
        wchar_t* appData = nullptr;
        size_t appDataLength = 0;
        const errno_t envResult = _wdupenv_s(&appData, &appDataLength, L"APPDATA");
        if (envResult == 0 && appData != nullptr && appDataLength > 0)
        {
            themePath = std::wstring(appData) + L"\\SimpleSpaces\\Spaces\\themes\\custom";
            free(appData);
        }
        else
        {
            if (appData != nullptr)
            {
                free(appData);
            }
            return false;
        }
    }

    // Attempt to load theme from directory
    UniversalThemeData theme;
    if (!UniversalThemeLoader::LoadFromDirectory(themePath, theme))
    {
        // Fallback to loading a built-in theme from application theme directory
        // This would typically be in the exe directory or a resources path
        return false;
    }

    // Cache the loaded theme
    m_cachedTheme = std::make_shared<UniversalThemeData>(theme);
    outTheme = m_cachedTheme;
    return true;
}

ThemePalette ThemePlatform::BuildPalette() const
{
    const ThemeStyle style = ResolveStyle();

    std::optional<ThemeMode> forcedMode;
    if (m_store)
    {
        const std::wstring modeSetting = m_store->Get(L"appearance.theme.mode", L"system");
        if (modeSetting == L"dark")
        {
            forcedMode = ThemeMode::Dark;
        }
        else if (modeSetting == L"light")
        {
            forcedMode = ThemeMode::Light;
        }
    }

    // Try to load and apply universal theme semantic tokens
    std::shared_ptr<UniversalThemeData> universalTheme;
    if (TryLoadUniversalTheme(universalTheme))
    {
        ThemePalette palette;
        
        // Resolve all semantic tokens to actual colors
        palette.windowColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.window.background", RGB(24, 24, 24));
        palette.surfaceColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.surface.background", RGB(36, 36, 36));
        palette.navColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.nav.background", RGB(30, 30, 30));
        palette.textColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.text.primary", RGB(232, 232, 232));
        palette.subtleTextColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.text.secondary", RGB(180, 180, 180));
        palette.accentColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.ui.accent", RGB(88, 101, 242));
        palette.borderColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "core.border.default", RGB(72, 72, 72));
        const COLORREF defaultSpaceTitleBarColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "space.titlebar.background", RGB(58, 58, 58));
        const COLORREF defaultSpaceTitleTextColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "space.titlebar.text", RGB(239, 239, 239));
        const COLORREF defaultSpaceItemTextColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "space.item.text", RGB(210, 210, 210));
        const COLORREF defaultSpaceItemHoverColor = UniversalThemeLoader::ResolveSemanticColorRef(
            *universalTheme, "space.item.hover", RGB(82, 82, 82));

        palette.spaceTitleBarColor = ResolveComponentColor(*universalTheme, "fence.default.titlebarBg")
            .value_or(defaultSpaceTitleBarColor);
        palette.spaceTitleTextColor = ResolveComponentColor(*universalTheme, "fence.default.titlebarText")
            .value_or(defaultSpaceTitleTextColor);
        palette.spaceItemTextColor = ResolveComponentColor(*universalTheme, "fence.default.itemAreaText")
            .value_or(defaultSpaceItemTextColor);
        palette.spaceItemHoverColor = ResolveComponentColor(*universalTheme, "fence.default.itemAreaHoverBg")
            .value_or(defaultSpaceItemHoverColor);

        return palette;
    }

    ThemePalette palette = (style == ThemeStyle::Win32ThemeCatalog && m_store)
        ? BuildWin32ThemePalette(m_store->Get(L"theme.win32.theme_id", L"graphite-office"), forcedMode)
        : BuildPaletteFor(ResolveMode(), style);

    return palette;
}

ThemeIconMapping ThemePlatform::ResolveIconMapping(const std::wstring& iconKey,
                                                   const std::wstring& fallbackGlyph) const
{
    ThemeIconMapping mapping;
    mapping.iconKey = iconKey;

    if (iconKey.empty())
    {
        mapping.glyph = fallbackGlyph;
        return mapping;
    }

    const auto& builtinGlyphs = BuiltinFallbackGlyphs();
    if (!fallbackGlyph.empty())
    {
        mapping.glyph = fallbackGlyph;
    }
    else
    {
        const auto glyphIt = builtinGlyphs.find(iconKey);
        mapping.glyph = (glyphIt != builtinGlyphs.end()) ? glyphIt->second : L"\uE943";
    }

    std::wstring selectedPack = L"tabler";
    if (m_store)
    {
        const std::wstring configuredPack = m_store->Get(L"appearance.icons.pack", L"tabler");
        if (!configuredPack.empty())
        {
            selectedPack = configuredPack;
        }

        const std::wstring perIconGlyph = m_store->Get(L"appearance.icons.glyph." + iconKey, L"");
        if (!perIconGlyph.empty())
        {
            mapping.glyph = perIconGlyph;
        }
    }

    std::shared_ptr<UniversalThemeData> universalTheme;
    if (TryLoadUniversalTheme(universalTheme))
    {
        const std::string packUtf8 = WStringToUtf8(selectedPack);
        const auto packIt = universalTheme->iconPackMappings.find(packUtf8);
        if (packIt != universalTheme->iconPackMappings.end())
        {
            const auto iconIt = packIt->second.find(WStringToUtf8(iconKey));
            if (iconIt != packIt->second.end())
            {
                mapping.assetPack = selectedPack;
                mapping.assetName = Utf8ToWString(iconIt->second);
                const std::wstring assetGlyph = GlyphForAssetName(mapping.assetName);
                if (!assetGlyph.empty())
                {
                    mapping.glyph = assetGlyph;
                }
                return mapping;
            }
        }
    }

    if (selectedPack == L"tabler")
    {
        const auto& builtinTabler = BuiltinTablerAssets();
        const auto assetIt = builtinTabler.find(iconKey);
        if (assetIt != builtinTabler.end())
        {
            mapping.assetPack = L"tabler";
            mapping.assetName = assetIt->second;
            const std::wstring assetGlyph = GlyphForAssetName(mapping.assetName);
            if (!assetGlyph.empty())
            {
                mapping.glyph = assetGlyph;
            }
        }
    }

    return mapping;
}

bool ThemePlatform::ExportCustomPreset(const std::wstring& filePath) const
{
    if (!m_store || filePath.empty())
    {
        return false;
    }

    try
    {
        const ThemePalette palette = BuildPalette();
        nlohmann::json root;
        root["version"] = 1;
        root["type"] = "simplespaces.theme-preset";
        root["mode"] = WStringToUtf8(m_store->Get(L"appearance.theme.mode", L"system"));
        root["style"] = "custom";
        root["textScalePercent"] = GetTextScalePercent();
        root["colors"] = {
            {"window", WStringToUtf8(m_store->Get(L"appearance.theme.custom.window", ColorToHex(palette.windowColor)))},
            {"surface", WStringToUtf8(m_store->Get(L"appearance.theme.custom.surface", ColorToHex(palette.surfaceColor)))},
            {"nav", WStringToUtf8(m_store->Get(L"appearance.theme.custom.nav", ColorToHex(palette.navColor)))},
            {"text", WStringToUtf8(m_store->Get(L"appearance.theme.custom.text", ColorToHex(palette.textColor)))},
            {"subtleText", WStringToUtf8(m_store->Get(L"appearance.theme.custom.subtle_text", ColorToHex(palette.subtleTextColor)))},
            {"accent", WStringToUtf8(m_store->Get(L"appearance.theme.custom.accent", ColorToHex(palette.accentColor)))},
            {"border", WStringToUtf8(m_store->Get(L"appearance.theme.custom.border", ColorToHex(palette.borderColor)))},
            {"spaceTitleBar", WStringToUtf8(m_store->Get(L"appearance.theme.custom.space_title_bar", ColorToHex(palette.spaceTitleBarColor)))},
            {"spaceTitleText", WStringToUtf8(m_store->Get(L"appearance.theme.custom.space_title_text", ColorToHex(palette.spaceTitleTextColor)))},
            {"spaceItemText", WStringToUtf8(m_store->Get(L"appearance.theme.custom.space_item_text", ColorToHex(palette.spaceItemTextColor)))},
            {"spaceItemHover", WStringToUtf8(m_store->Get(L"appearance.theme.custom.space_item_hover", ColorToHex(palette.spaceItemHoverColor)))}
        };

        std::ofstream output(filePath);
        if (!output.is_open())
        {
            return false;
        }

        output << root.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ThemePlatform::ImportCustomPreset(const std::wstring& filePath) const
{
    if (!m_store || filePath.empty())
    {
        return false;
    }

    try
    {
        std::ifstream input(filePath);
        if (!input.is_open())
        {
            return false;
        }

        nlohmann::json root;
        input >> root;

        const auto& colors = root["colors"];
        if (!colors.is_object())
        {
            return false;
        }

        auto importColor = [&](const char* jsonKey, const std::wstring& storeKey) {
            if (colors.contains(jsonKey) && colors[jsonKey].is_string())
            {
                m_store->Set(storeKey, Utf8ToWString(colors[jsonKey].get<std::string>()));
            }
        };

        if (root.contains("mode") && root["mode"].is_string())
        {
            m_store->Set(L"appearance.theme.mode", Utf8ToWString(root["mode"].get<std::string>()));
        }
        if (root.contains("textScalePercent") && root["textScalePercent"].is_number_integer())
        {
            const std::wstring scale = std::to_wstring(root["textScalePercent"].get<int>());
            m_store->Set(L"appearance.theme.text_scale_percent", scale);
            m_store->Set(L"appearance.text.scale_percent", scale);
        }

        importColor("window", L"appearance.theme.custom.window");
        importColor("surface", L"appearance.theme.custom.surface");
        importColor("nav", L"appearance.theme.custom.nav");
        importColor("text", L"appearance.theme.custom.text");
        importColor("subtleText", L"appearance.theme.custom.subtle_text");
        importColor("accent", L"appearance.theme.custom.accent");
        importColor("border", L"appearance.theme.custom.border");
        importColor("spaceTitleBar", L"appearance.theme.custom.space_title_bar");
        importColor("spaceTitleText", L"appearance.theme.custom.space_title_text");
        importColor("spaceItemText", L"appearance.theme.custom.space_item_text");
        importColor("spaceItemHover", L"appearance.theme.custom.space_item_hover");

        m_store->Set(L"appearance.theme.style", L"custom");
        return true;
    }
    catch (...)
    {
        return false;
    }
}

UINT ThemePlatform::GetThemeChangedMessageId()
{
    static const UINT kThemeChanged = RegisterWindowMessageW(L"SimpleSpaces.ThemeChanged");
    return kThemeChanged;
}

