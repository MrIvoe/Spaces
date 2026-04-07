#include "core/ThemePlatform.h"

#include "core/SettingsStore.h"

#include <nlohmann/json.hpp>

#include <cwctype>
#include <fstream>
#include <optional>
#include <string>

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
            palette.fenceTitleBarColor = RGB(39, 45, 53);
            palette.fenceTitleTextColor = RGB(199, 210, 223);
            palette.fenceItemTextColor = RGB(173, 186, 199);
            palette.fenceItemHoverColor = RGB(55, 63, 72);
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
            palette.fenceTitleBarColor = RGB(234, 238, 242);
            palette.fenceTitleTextColor = RGB(31, 35, 40);
            palette.fenceItemTextColor = RGB(36, 41, 47);
            palette.fenceItemHoverColor = RGB(230, 236, 241);
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

        palette.fenceTitleBarColor = BlendColor(palette.fenceTitleBarColor, palette.accentColor, dark ? 120 : 90);
        palette.borderColor = BlendColor(palette.borderColor, palette.accentColor, 72);
        palette.fenceItemHoverColor = BlendColor(palette.fenceItemHoverColor, palette.accentColor, dark ? 55 : 40);
        return palette;
    }
}

ThemePlatform::ThemePlatform(SettingsStore* store)
    : m_store(store)
{
}

void ThemePlatform::SetStore(SettingsStore* store)
{
    m_store = store;
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
        const std::wstring raw = m_store->Get(L"appearance.theme.text_scale_percent", L"115");
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

FencePolicyDefaults ThemePlatform::ResolveFencePolicyDefaults() const
{
    FencePolicyDefaults defaults;
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

        palette.fenceTitleBarColor = RGB(58, 58, 58);
        palette.fenceTitleTextColor = RGB(239, 239, 239);
        palette.fenceItemTextColor = RGB(210, 210, 210);
        palette.fenceItemHoverColor = RGB(82, 82, 82);
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

        palette.fenceTitleBarColor = RGB(216, 226, 235);
        palette.fenceTitleTextColor = RGB(30, 42, 54);
        palette.fenceItemTextColor = RGB(43, 58, 72);
        palette.fenceItemHoverColor = RGB(201, 219, 235);
    }

    (void)style;
    return palette;
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

    ThemePalette palette = (style == ThemeStyle::Win32ThemeCatalog && m_store)
        ? BuildWin32ThemePalette(m_store->Get(L"theme.win32.theme_id", L"graphite-office"), forcedMode)
        : BuildPaletteFor(ResolveMode(), style);

    return palette;
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
        root["type"] = "simplefences.theme-preset";
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
            {"fenceTitleBar", WStringToUtf8(m_store->Get(L"appearance.theme.custom.fence_title_bar", ColorToHex(palette.fenceTitleBarColor)))},
            {"fenceTitleText", WStringToUtf8(m_store->Get(L"appearance.theme.custom.fence_title_text", ColorToHex(palette.fenceTitleTextColor)))},
            {"fenceItemText", WStringToUtf8(m_store->Get(L"appearance.theme.custom.fence_item_text", ColorToHex(palette.fenceItemTextColor)))},
            {"fenceItemHover", WStringToUtf8(m_store->Get(L"appearance.theme.custom.fence_item_hover", ColorToHex(palette.fenceItemHoverColor)))}
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
            m_store->Set(L"appearance.theme.text_scale_percent", std::to_wstring(root["textScalePercent"].get<int>()));
        }

        importColor("window", L"appearance.theme.custom.window");
        importColor("surface", L"appearance.theme.custom.surface");
        importColor("nav", L"appearance.theme.custom.nav");
        importColor("text", L"appearance.theme.custom.text");
        importColor("subtleText", L"appearance.theme.custom.subtle_text");
        importColor("accent", L"appearance.theme.custom.accent");
        importColor("border", L"appearance.theme.custom.border");
        importColor("fenceTitleBar", L"appearance.theme.custom.fence_title_bar");
        importColor("fenceTitleText", L"appearance.theme.custom.fence_title_text");
        importColor("fenceItemText", L"appearance.theme.custom.fence_item_text");
        importColor("fenceItemHover", L"appearance.theme.custom.fence_item_hover");

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
    static const UINT kThemeChanged = RegisterWindowMessageW(L"SimpleFences.ThemeChanged");
    return kThemeChanged;
}

