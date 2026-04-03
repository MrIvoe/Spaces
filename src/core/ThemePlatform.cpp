#include "core/ThemePlatform.h"

#include "core/SettingsStore.h"

#include <nlohmann/json.hpp>

#include <cwctype>
#include <fstream>
#include <optional>
#include <string>

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
    ThemeMode mode = DetectSystemMode();

    if (!m_store)
    {
        return mode;
    }

    const std::wstring modeValue = m_store->Get(L"appearance.theme.mode", L"system");
    if (modeValue == L"dark")
    {
        mode = ThemeMode::Dark;
    }
    else if (modeValue == L"light")
    {
        mode = ThemeMode::Light;
    }

    return mode;
}

ThemeStyle ThemePlatform::ResolveStyle() const
{
    if (!m_store)
    {
        return ThemeStyle::System;
    }

    const std::wstring style = m_store->Get(L"appearance.theme.style", L"system");
    if (style == L"discord")
    {
        return ThemeStyle::Discord;
    }
    if (style == L"fences")
    {
        return ThemeStyle::Fences;
    }
    if (style == L"github_dark")
    {
        return ThemeStyle::GitHubDark;
    }
    if (style == L"github_dark_dimmed")
    {
        return ThemeStyle::GitHubDarkDimmed;
    }
    if (style == L"github_light")
    {
        return ThemeStyle::GitHubLight;
    }
    if (style == L"custom")
    {
        return ThemeStyle::Custom;
    }
    return ThemeStyle::System;
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

    switch (ResolveStyle())
    {
    case ThemeStyle::Discord:
        defaults.rollupWhenNotHovered = false;
        defaults.transparentWhenNotHovered = true;
        defaults.labelsOnHover = true;
        defaults.iconSpacingPreset = L"comfortable";
        break;
    case ThemeStyle::Fences:
        defaults.rollupWhenNotHovered = false;
        defaults.transparentWhenNotHovered = false;
        defaults.labelsOnHover = false;
        defaults.iconSpacingPreset = L"compact";
        break;
    case ThemeStyle::GitHubDark:
    case ThemeStyle::GitHubDarkDimmed:
        defaults.rollupWhenNotHovered = true;
        defaults.transparentWhenNotHovered = false;
        defaults.labelsOnHover = true;
        defaults.iconSpacingPreset = L"comfortable";
        break;
    case ThemeStyle::GitHubLight:
        defaults.rollupWhenNotHovered = false;
        defaults.transparentWhenNotHovered = false;
        defaults.labelsOnHover = true;
        defaults.iconSpacingPreset = L"spacious";
        break;
    case ThemeStyle::Custom:
        defaults.rollupWhenNotHovered = false;
        defaults.transparentWhenNotHovered = false;
        defaults.labelsOnHover = true;
        defaults.iconSpacingPreset = L"comfortable";
        break;
    case ThemeStyle::System:
    default:
        defaults.rollupWhenNotHovered = false;
        defaults.transparentWhenNotHovered = false;
        defaults.labelsOnHover = true;
        defaults.iconSpacingPreset = L"comfortable";
        break;
    }

    if (m_store)
    {
        const auto applyTogglePolicy = [&](const std::wstring& key, bool& target) {
            const std::wstring value = m_store->Get(key, L"auto");
            if (value == L"on")
            {
                target = true;
            }
            else if (value == L"off")
            {
                target = false;
            }
        };

        applyTogglePolicy(L"appearance.theme.policy.rollup_default", defaults.rollupWhenNotHovered);
        applyTogglePolicy(L"appearance.theme.policy.transparency_default", defaults.transparentWhenNotHovered);
        applyTogglePolicy(L"appearance.theme.policy.labels_on_hover_default", defaults.labelsOnHover);

        const std::wstring spacing = m_store->Get(L"appearance.theme.policy.spacing_preset_default", L"auto");
        if (spacing == L"compact" || spacing == L"comfortable" || spacing == L"spacious")
        {
            defaults.iconSpacingPreset = spacing;
        }
    }

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

    if (style == ThemeStyle::Discord)
    {
        if (mode == ThemeMode::Dark)
        {
            palette.windowColor = RGB(47, 49, 54);
            palette.surfaceColor = RGB(54, 57, 63);
            palette.navColor = RGB(32, 34, 37);
            palette.textColor = RGB(220, 221, 222);
            palette.subtleTextColor = RGB(160, 162, 168);
            palette.accentColor = RGB(88, 101, 242);
            palette.borderColor = RGB(43, 45, 49);

            palette.fenceTitleBarColor = RGB(40, 43, 48);
            palette.fenceTitleTextColor = RGB(229, 230, 232);
            palette.fenceItemTextColor = RGB(211, 214, 218);
            palette.fenceItemHoverColor = RGB(70, 74, 82);
        }
        else
        {
            palette.windowColor = RGB(243, 245, 247);
            palette.surfaceColor = RGB(255, 255, 255);
            palette.navColor = RGB(232, 236, 240);
            palette.textColor = RGB(46, 51, 56);
            palette.subtleTextColor = RGB(106, 115, 128);
            palette.accentColor = RGB(88, 101, 242);
            palette.borderColor = RGB(214, 220, 226);

            palette.fenceTitleBarColor = RGB(225, 232, 240);
            palette.fenceTitleTextColor = RGB(45, 50, 56);
            palette.fenceItemTextColor = RGB(50, 58, 66);
            palette.fenceItemHoverColor = RGB(209, 218, 227);
        }
    }
    else if (style == ThemeStyle::Fences)
    {
        if (mode == ThemeMode::Dark)
        {
            palette.windowColor = RGB(22, 27, 34);
            palette.surfaceColor = RGB(28, 34, 42);
            palette.navColor = RGB(20, 24, 31);
            palette.textColor = RGB(225, 231, 238);
            palette.subtleTextColor = RGB(167, 177, 188);
            palette.accentColor = RGB(64, 168, 255);
            palette.borderColor = RGB(57, 73, 88);

            palette.fenceTitleBarColor = RGB(35, 50, 66);
            palette.fenceTitleTextColor = RGB(230, 238, 246);
            palette.fenceItemTextColor = RGB(209, 220, 232);
            palette.fenceItemHoverColor = RGB(50, 72, 94);
        }
        else
        {
            palette.windowColor = RGB(236, 242, 248);
            palette.surfaceColor = RGB(248, 251, 255);
            palette.navColor = RGB(227, 236, 245);
            palette.textColor = RGB(27, 35, 42);
            palette.subtleTextColor = RGB(90, 104, 118);
            palette.accentColor = RGB(49, 142, 226);
            palette.borderColor = RGB(177, 200, 222);

            palette.fenceTitleBarColor = RGB(191, 216, 238);
            palette.fenceTitleTextColor = RGB(28, 45, 62);
            palette.fenceItemTextColor = RGB(34, 56, 76);
            palette.fenceItemHoverColor = RGB(209, 227, 245);
        }
    }
    else if (style == ThemeStyle::GitHubDark)
    {
        palette.windowColor = RGB(13, 17, 23);
        palette.surfaceColor = RGB(22, 27, 34);
        palette.navColor = RGB(1, 4, 9);
        palette.textColor = RGB(230, 237, 243);
        palette.subtleTextColor = RGB(139, 148, 158);
        palette.accentColor = RGB(47, 129, 247);
        palette.borderColor = RGB(48, 54, 61);
        palette.fenceTitleBarColor = RGB(22, 27, 34);
        palette.fenceTitleTextColor = RGB(230, 237, 243);
        palette.fenceItemTextColor = RGB(201, 209, 217);
        palette.fenceItemHoverColor = RGB(33, 38, 45);
    }
    else if (style == ThemeStyle::GitHubDarkDimmed)
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
    else if (style == ThemeStyle::GitHubLight)
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

    return palette;
}

ThemePalette ThemePlatform::BuildPalette() const
{
    ThemePalette palette = BuildPaletteFor(ResolveMode(), ResolveStyle());

    if (ResolveStyle() == ThemeStyle::Custom && m_store)
    {
        auto apply = [&](const std::wstring& key, COLORREF& target) {
            const auto parsed = ParseHexColor(m_store->Get(key, L""));
            if (parsed.has_value())
            {
                target = *parsed;
            }
        };

        apply(L"appearance.theme.custom.window", palette.windowColor);
        apply(L"appearance.theme.custom.surface", palette.surfaceColor);
        apply(L"appearance.theme.custom.nav", palette.navColor);
        apply(L"appearance.theme.custom.text", palette.textColor);
        apply(L"appearance.theme.custom.subtle_text", palette.subtleTextColor);
        apply(L"appearance.theme.custom.accent", palette.accentColor);
        apply(L"appearance.theme.custom.border", palette.borderColor);
        apply(L"appearance.theme.custom.fence_title_bar", palette.fenceTitleBarColor);
        apply(L"appearance.theme.custom.fence_title_text", palette.fenceTitleTextColor);
        apply(L"appearance.theme.custom.fence_item_text", palette.fenceItemTextColor);
        apply(L"appearance.theme.custom.fence_item_hover", palette.fenceItemHoverColor);
    }

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
