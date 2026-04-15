#include "ui/SettingsWindow.h"
#include "ui/VirtualNavList.h"
#include "ui/SwitchControl.h"

#include "core/PluginCatalogFetcher.h"
#include "core/PluginHubSync.h"
#include "core/PluginPackageInstaller.h"
#include "core/ThemePackageLoader.h"
#include "core/ThemePackageValidator.h"
#include "core/ThemePlatform.h"
#include "extensions/PluginSettingsRegistry.h"
#include "AppResources.h"
#include "Win32Helpers.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <commctrl.h>
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "Dwmapi.lib")

namespace
{
    #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
    #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
    #endif
    #ifndef DWMWA_WINDOW_CORNER_PREFERENCE
    #define DWMWA_WINDOW_CORNER_PREFERENCE 33
    #endif

    enum class DwmWindowCornerPreference
    {
        Default = 0,
        DoNotRound = 1,
        Round = 2,
        RoundSmall = 3
    };

    constexpr int kHeaderTitleHeight = 24;
    constexpr int kHeaderSubtitleHeight = 18;
    constexpr int kHeaderGap = 4;
    constexpr int kMenuBarHeight = 24;
    constexpr int kMenuBarGap = 6;
    constexpr int kSearchRowHeight = 28;
    constexpr int kSearchRowGap = 6;
    constexpr int kStatusHeight = 24;

    constexpr int TopAreaHeight()
    {
        return kMenuBarHeight + kMenuBarGap + kHeaderTitleHeight + kHeaderSubtitleHeight + kHeaderGap + kSearchRowHeight + kSearchRowGap + 16;
    }

    // Returns true when the active theme is the Neon Cyberpunk deep-space theme.
    // Detected by its signature canvas color: #09 0B 11.
    bool IsCyberTheme(COLORREF windowColor)
    {
        return GetRValue(windowColor) == 9 && GetGValue(windowColor) == 11 && GetBValue(windowColor) == 17;
    }

    void ApplyModernWindowChrome(HWND hwnd, bool darkMode)
    {
        if (!hwnd)
        {
            return;
        }

        const BOOL useDark = darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

        const auto rounded = static_cast<DWORD>(DwmWindowCornerPreference::Round);
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &rounded, sizeof(rounded));
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

    bool StartsWith(const std::wstring& text, const std::wstring& prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    std::wstring JoinCapabilities(const std::vector<std::wstring>& capabilities)
    {
        if (capabilities.empty())
        {
            return L"none";
        }

        std::wstring text;
        for (size_t i = 0; i < capabilities.size(); ++i)
        {
            if (i > 0)
            {
                text += L", ";
            }
            text += capabilities[i];
        }
        return text;
    }

    std::wstring PluginStateText(const PluginStatusView& plugin)
    {
        if (!plugin.enabled)
        {
            return L"disabled";
        }
        return plugin.loaded ? L"loaded" : L"failed";
    }

    std::wstring PluginStateBadge(const PluginStatusView& plugin)
    {
        if (!plugin.enabled)
        {
            return L"[Disabled]";
        }

        if (plugin.compatibilityStatus == L"incompatible" || plugin.compatibilityStatus == L"rejected")
        {
            return L"[Incompatible]";
        }

        if (plugin.loaded)
        {
            return L"[Loaded]";
        }

        return L"[Failed]";
    }

    std::wstring ToLowerCopy(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return text;
    }

    bool IsContentChipId(int id)
    {
        return id == 112 || id == 113 || id == 114 || id == 115;
    }

    bool MatchesPluginStatusFilter(const PluginStatusView& plugin, const std::wstring& statusFilter)
    {
        if (statusFilter.empty() || statusFilter == L"all")
        {
            return true;
        }

        if (statusFilter == L"loaded")
        {
            return plugin.enabled && plugin.loaded;
        }

        if (statusFilter == L"failed")
        {
            return plugin.enabled && !plugin.loaded;
        }

        if (statusFilter == L"disabled")
        {
            return !plugin.enabled;
        }

        if (statusFilter == L"incompatible")
        {
            return plugin.compatibilityStatus == L"incompatible" || plugin.compatibilityStatus == L"rejected";
        }

        return true;
    }

    bool MatchesPluginTextFilter(const PluginStatusView& plugin, const std::wstring& textFilter)
    {
        if (textFilter.empty())
        {
            return true;
        }

        const std::wstring query = ToLowerCopy(textFilter);
        const std::wstring id = ToLowerCopy(plugin.id);
        const std::wstring name = ToLowerCopy(plugin.displayName);
        return id.find(query) != std::wstring::npos || name.find(query) != std::wstring::npos;
    }

    bool HasZipExtension(const std::wstring& filePath)
    {
        const std::wstring ext = ToLowerCopy(std::filesystem::path(filePath).extension().wstring());
        return ext == L".zip";
    }

    std::wstring ResolveDefaultCatalogSource()
    {
        return PluginCatalog::CatalogFetcher::GetDefaultCatalogSource();
    }

    std::wstring ResolveConfiguredCatalogSource(const PluginSettingsRegistry* settingsRegistry)
    {
        if (settingsRegistry)
        {
            const std::wstring configured = settingsRegistry->GetValue(L"settings.plugins.catalog_url", L"");
            if (!configured.empty())
            {
                return configured;
            }
        }

        return L"";
    }

    bool TryFetchCatalogWithFallback(const PluginSettingsRegistry* settingsRegistry,
                                     PluginCatalog::CatalogFetcher& fetcher,
                                     std::wstring& resolvedSource,
                                     std::wstring& errorText)
    {
        const std::wstring configured = ResolveConfiguredCatalogSource(settingsRegistry);
        const std::wstring fallback = ResolveDefaultCatalogSource();

        bool usedFallback = false;
        if (fetcher.FetchCatalogWithFallback(configured, fallback, &resolvedSource, &usedFallback))
        {
            errorText.clear();
            return true;
        }

        if (configured.empty() && fallback.empty())
        {
            errorText = L"No catalog source is configured and no local catalog file was found.";
        }
        else
        {
            errorText = fetcher.GetLastError();
        }

        resolvedSource.clear();
        return false;
    }

    std::wstring TruncateWithEllipsis(const std::wstring& text, size_t maxLength)
    {
        if (text.size() <= maxLength)
        {
            return text;
        }
        if (maxLength < 4)
        {
            return text.substr(0, maxLength);
        }
        return text.substr(0, maxLength - 3) + L"...";
    }

    bool MarketplaceEntryMatchesQuery(const PluginCatalog::PluginEntry& entry, const std::wstring& query)
    {
        if (query.empty())
        {
            return true;
        }

        const std::wstring q = ToLowerCopy(query);
        if (ToLowerCopy(entry.id).find(q) != std::wstring::npos)
        {
            return true;
        }
        if (ToLowerCopy(entry.displayName).find(q) != std::wstring::npos)
        {
            return true;
        }
        if (ToLowerCopy(entry.description).find(q) != std::wstring::npos)
        {
            return true;
        }
        if (ToLowerCopy(entry.author).find(q) != std::wstring::npos)
        {
            return true;
        }
        if (ToLowerCopy(entry.category).find(q) != std::wstring::npos)
        {
            return true;
        }

        for (const auto& capability : entry.capabilities)
        {
            if (ToLowerCopy(capability).find(q) != std::wstring::npos)
            {
                return true;
            }
        }

        for (const auto& tag : entry.tags)
        {
            if (ToLowerCopy(tag).find(q) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }

    std::string NarrowAscii(const std::wstring& text)
    {
        std::string result;
        result.reserve(text.size());
        for (wchar_t c : text)
        {
            result.push_back((c >= 0 && c <= 0x7F) ? static_cast<char>(c) : '?');
        }
        return result;
    }

    void AddColorTokenIfPresent(nlohmann::json& colors,
                                const ThemePackageLoader::TokenMap& tokenMap,
                                const std::wstring& tokenName,
                                const char* presetKey)
    {
        std::wstring value;
        if (tokenMap.GetToken(tokenName, value) && !value.empty())
        {
            colors[presetKey] = NarrowAscii(value);
        }
    }

    bool BuildPresetJsonFromTokenMap(const ThemePackageLoader::TokenMap& tokenMap,
                                     const std::wstring& outputPath)
    {
        nlohmann::json root;
        root["version"] = 1;
        root["type"] = "simplespaces.theme-preset";
        root["mode"] = "system";
        root["style"] = "custom";
        root["textScalePercent"] = 100;

        nlohmann::json colors = nlohmann::json::object();
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.window_color", "window");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.surface_color", "surface");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.nav_color", "nav");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.text_color", "text");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.subtle_text_color", "subtleText");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.accent_color", "accent");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.base.border_color", "border");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.space.title_bar_color", "spaceTitleBar");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.space.title_text_color", "spaceTitleText");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.space.item_text_color", "spaceItemText");
        AddColorTokenIfPresent(colors, tokenMap, L"win32.space.item_hover_color", "spaceItemHover");

        if (colors.empty())
        {
            return false;
        }

        root["colors"] = colors;

        std::ofstream output(outputPath);
        if (!output.is_open())
        {
            return false;
        }

        output << root.dump(2);
        return true;
    }
}

bool SettingsWindow::ShowScaffold(const std::vector<SettingsPageView>& pages,
                                   const std::vector<PluginStatusView>& plugins,
                                   PluginSettingsRegistry* registry,
                                   const ThemePlatform* themePlatform)
{
    m_settingsRegistry = registry;
    m_themePlatform = themePlatform;
    m_navCollapsed = (m_settingsRegistry && m_settingsRegistry->GetValue(L"settings.ui.nav_collapsed", L"false") == L"true");
    m_showAdvancedSettings = (m_settingsRegistry && m_settingsRegistry->GetValue(L"settings.ui.advanced_mode", L"false") == L"true");
    m_plugins = plugins;
    m_pages = BuildPages(pages, plugins);
    if (m_pages.empty())
    {
        m_pages.push_back(UiPage{L"general", L"builtin.settings", L"General",
                                  L"No settings pages are currently available.", {}});
    }
    BuildPluginTabs(plugins);

    if (!EnsureWindow())
    {
        return false;
    }

    RefreshTheme();
    PopulatePluginTabs();
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(m_hwnd);

    if (m_settingsRegistry &&
        m_settingsRegistry->GetValue(L"settings.accessibility.keyboard_first", L"false") == L"true" &&
        m_navList)
    {
        SetFocus(m_navList->GetHwnd());
    }

    const std::wstring themeText = (m_themeMode == ThemeMode::Dark) ? L"dark" : L"light";
    Win32Helpers::LogInfo(L"Settings window opened (theme='" + themeText + L"').");

    return true;
}

bool SettingsWindow::EnsureWindow()
{
    if (m_hwnd && IsWindow(m_hwnd))
    {
        return true;
    }

    if (m_hwnd && !IsWindow(m_hwnd))
    {
        Win32Helpers::LogError(L"Settings window handle was stale; recreating window.");
        m_hwnd = nullptr;
        m_navToggleButton = nullptr;
        m_menuFileButton = nullptr;
        m_menuEditButton = nullptr;
        m_menuViewButton = nullptr;
        m_menuPluginsButton = nullptr;
        m_contentSearchEdit = nullptr;
        m_chipAllButton = nullptr;
        m_chipToggleButton = nullptr;
        m_chipChoiceButton = nullptr;
        m_chipTextButton = nullptr;
        m_marketplaceDiscoverTabButton = nullptr;
        m_marketplaceInstalledTabButton = nullptr;
        m_pluginTreeView = nullptr;
        m_headerTitle = nullptr;
        m_headerSubtitle = nullptr;
        m_headerHelpButton = nullptr;
        delete m_navList;
        m_navList = nullptr;
        m_pageView = nullptr;
        m_rightScrollPanel = nullptr;
        m_statusBar = nullptr;
        m_tooltip = nullptr;
        m_fieldControls.clear();
        m_fieldControlLayouts.clear();
        m_controlFieldMap.clear();
        m_sectionCardRects.clear();
        m_rightPaneTextStatics.clear();
    }

    static const wchar_t* kClassName = L"SimpleSpaces_SettingsWindow";
    static const wchar_t* kScrollPanelClass = L"SimpleSpaces_SettingsRightPane";

    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWindow::WndProcStatic;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_SPACES_APP));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
    {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            Win32Helpers::LogError(L"Settings window class registration failed: " + std::to_wstring(error));
            return false;
        }
    }

    WNDCLASSW scrollWc{};
    scrollWc.style = CS_HREDRAW | CS_VREDRAW;
    scrollWc.lpfnWndProc = SettingsWindow::ScrollPanelProc;
    scrollWc.hInstance = GetModuleHandleW(nullptr);
    scrollWc.lpszClassName = kScrollPanelClass;
    scrollWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    scrollWc.hbrBackground = nullptr;
    if (!RegisterClassW(&scrollWc))
    {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            Win32Helpers::LogError(L"Settings scroll panel class registration failed: " + std::to_wstring(error));
        }
    }

    if (!SwitchControl::Register(GetModuleHandleW(nullptr)))
    {
        Win32Helpers::LogError(L"Settings switch control class registration failed: " + std::to_wstring(GetLastError()));
    }

    m_hwnd = CreateWindowExW(
        0,
        kClassName,
        L"SimpleSpaces Settings",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        620,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (!m_hwnd)
    {
        Win32Helpers::LogError(L"Settings window CreateWindowEx failed: " + std::to_wstring(GetLastError()));
        return false;
    }

    const HINSTANCE hInstance = GetModuleHandleW(nullptr);
    const HICON bigIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SPACES_APP));
    const HICON smallIcon = static_cast<HICON>(LoadImageW(
        hInstance,
        MAKEINTRESOURCEW(IDI_SPACES_APP),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (bigIcon)
    {
        SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    }
    if (smallIcon)
    {
        SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }

    return true;
}

void SettingsWindow::DestroyThemeBrushes()
{
    if (m_windowBrush)
    {
        DeleteObject(m_windowBrush);
        m_windowBrush = nullptr;
    }

    if (m_surfaceBrush)
    {
        DeleteObject(m_surfaceBrush);
        m_surfaceBrush = nullptr;
    }

    if (m_navBrush)
    {
        DeleteObject(m_navBrush);
        m_navBrush = nullptr;
    }
}

void SettingsWindow::DestroyFonts()
{
    if (m_baseFont)
    {
        DeleteObject(m_baseFont);
        m_baseFont = nullptr;
    }
    if (m_sectionFont)
    {
        DeleteObject(m_sectionFont);
        m_sectionFont = nullptr;
    }
    if (m_navFont)
    {
        DeleteObject(m_navFont);
        m_navFont = nullptr;
    }
    if (m_iconFont)
    {
        DeleteObject(m_iconFont);
        m_iconFont = nullptr;
    }
}

void SettingsWindow::RefreshTheme()
{
    // Single source of truth: delegate entirely to ThemePlatform.
    ThemePalette palette;
    int textScale = 115;

    if (m_themePlatform)
    {
        m_themeMode = m_themePlatform->ResolveMode();
        m_themeStyle = m_themePlatform->ResolveStyle();
        palette = m_themePlatform->BuildPalette();
        textScale = m_themePlatform->GetTextScalePercent();
    }
    else
    {
        // Fallback: detect system theme with a temporary platform (no settings store).
        ThemePlatform tmp;
        m_themeMode = tmp.ResolveMode();
        m_themeStyle = ThemeStyle::System;
        palette = tmp.BuildPalette();
    }

    textScale = (textScale < 90) ? 90 : (textScale > 150) ? 150 : textScale;

    m_windowColor = palette.windowColor;
    m_surfaceColor = palette.surfaceColor;
    m_navColor = palette.navColor;
    m_textColor = palette.textColor;
    m_subtleTextColor = palette.subtleTextColor;
    m_accentColor = palette.accentColor;

    DestroyThemeBrushes();
    DestroyFonts();
    m_windowBrush = CreateSolidBrush(m_windowColor);
    m_surfaceBrush = CreateSolidBrush(m_surfaceColor);
    m_navBrush = CreateSolidBrush(m_navColor);
    ApplyWindowTranslucency();
    ApplyModernWindowChrome(m_hwnd, m_themeMode == ThemeMode::Dark);

    // Keep settings content readable and consistent: 12px baseline regardless of
    // global theme text scaling.
    const int basePx = 12;
    const int sectionPx = 13;
    const int navPx = 12;

    int iconScalePercent = textScale;
    if (m_settingsRegistry)
    {
        const std::wstring iconSize = m_settingsRegistry->GetValue(L"appearance.ui.icon_size", L"normal");
        if (iconSize == L"smaller")
        {
            iconScalePercent = 85;
        }
        else if (iconSize == L"small" || iconSize == L"sm")
        {
            iconScalePercent = 95;
        }
        else if (iconSize == L"normal" || iconSize == L"md")
        {
            iconScalePercent = 100;
        }
        else if (iconSize == L"large" || iconSize == L"lg")
        {
            iconScalePercent = 115;
        }
    }

    const int iconPx = (20 * iconScalePercent) / 100;

    // Larger typography for accessibility and modern app feel.
    m_baseFont = CreateFontW(-basePx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_sectionFont = CreateFontW(-sectionPx, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
    m_navFont = CreateFontW(-navPx, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    m_iconFont = CreateFontW(-iconPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!m_iconFont)
    {
        m_iconFont = CreateFontW(-iconPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    if (m_pageView)
    {
        SendMessageW(m_pageView, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_navList)
    {
        SendMessageW(m_navList->GetHwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
        m_navList->SetFonts(m_navFont ? m_navFont : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),
                            m_iconFont ? m_iconFont : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    }
    if (m_headerTitle)
    {
        SendMessageW(m_headerTitle, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_sectionFont ? m_sectionFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_headerSubtitle)
    {
        SendMessageW(m_headerSubtitle, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_menuFileButton)
    {
        SendMessageW(m_menuFileButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_menuEditButton)
    {
        SendMessageW(m_menuEditButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_menuViewButton)
    {
        SendMessageW(m_menuViewButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_menuPluginsButton)
    {
        SendMessageW(m_menuPluginsButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_contentSearchEdit)
    {
        SendMessageW(m_contentSearchEdit, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_chipAllButton)
    {
        SendMessageW(m_chipAllButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_modeBasicButton)
    {
        SendMessageW(m_modeBasicButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_modeAdvancedButton)
    {
        SendMessageW(m_modeAdvancedButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_chipToggleButton)
    {
        SendMessageW(m_chipToggleButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }

    for (HWND hwnd : m_fieldControls)
    {
        if (!hwnd)
        {
            continue;
        }

        const int ctrlId = GetDlgCtrlID(hwnd);
        const auto infoIt = m_controlFieldMap.find(ctrlId);
        if (infoIt == m_controlFieldMap.end() || infoIt->second.type != SettingsFieldType::Bool)
        {
            continue;
        }

        SwitchControl::Colors switchColors;
        switchColors.surface = m_surfaceColor;
        switchColors.accent = m_accentColor;
        switchColors.text = m_textColor;
        switchColors.window = m_windowColor;
        SwitchControl::SetColors(hwnd, switchColors);
    }
    if (m_chipChoiceButton)
    {
        SendMessageW(m_chipChoiceButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_chipTextButton)
    {
        SendMessageW(m_chipTextButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_statusBar)
    {
        SendMessageW(m_statusBar, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }

    if (m_hwnd)
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    if (m_navList)
    {
        m_navList->Invalidate();
    }
    if (m_pageView && IsWindowVisible(m_pageView))
    {
        InvalidateRect(m_pageView, nullptr, FALSE);
    }
    if (m_rightScrollPanel && IsWindowVisible(m_rightScrollPanel))
    {
        InvalidateRect(m_rightScrollPanel, nullptr, FALSE);
    }
}

void SettingsWindow::ApplyWindowTranslucency()
{
    if (!m_hwnd)
    {
        return;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) != 0)
    {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    DWM_BLURBEHIND bb{};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = FALSE;
    DwmEnableBlurBehindWindow(m_hwnd, &bb);
}

std::wstring SettingsWindow::ResolvePluginDisplayName(const std::wstring& pluginId, const std::vector<PluginStatusView>& plugins) const
{
    if (pluginId == L"builtin.plugins")
    {
        return L"Plugins";
    }

    for (const auto& plugin : plugins)
    {
        if (plugin.id == pluginId)
        {
            return plugin.displayName.empty() ? plugin.id : plugin.displayName;
        }
    }

    if (pluginId.empty())
    {
        return L"General";
    }

    return pluginId;
}

std::wstring SettingsWindow::ResolvePluginIconKey(const PluginStatusView& plugin) const
{
    if (plugin.id == L"builtin.settings")
    {
        return L"plugins.builtin.settings";
    }
    if (plugin.id == L"builtin.tray")
    {
        return L"plugins.builtin.tray";
    }
    if (plugin.id == L"builtin.widgets")
    {
        return L"plugins.builtin.widgets";
    }
    if (plugin.id == L"builtin.explorer_portal")
    {
        return L"plugins.builtin.explorer_portal";
    }
    if (plugin.id == L"builtin.core_commands")
    {
        return L"plugins.builtin.core_commands";
    }

    for (const auto& capability : plugin.capabilities)
    {
        if (capability == L"settings_pages")
        {
            return L"settings.general";
        }
        if (capability == L"appearance")
        {
            return L"settings.appearance";
        }
        if (capability == L"widgets")
        {
            return L"plugins.builtin.widgets";
        }
        if (capability == L"space_content_provider")
        {
            return L"plugins.builtin.explorer_portal";
        }
        if (capability == L"tray_contributions")
        {
            return L"plugins.builtin.tray";
        }
    }

    return L"plugins.generic";
}

void SettingsWindow::BuildPluginTabs(const std::vector<PluginStatusView>& plugins)
{
    m_pluginTabs.clear();

    PluginTab overview;
    overview.pluginId = L"__overview__";
    overview.iconKey = L"settings.overview";
    if (m_themePlatform)
    {
        const ThemeIconMapping icon = m_themePlatform->ResolveIconMapping(overview.iconKey, L"\uE80F");
        overview.iconGlyph = icon.glyph;
        overview.iconAsset = icon.assetPack.empty() ? L"" : (icon.assetPack + L":" + icon.assetName);
    }
    else
    {
        overview.iconGlyph = L"\uE80F";
    }
    overview.title = L"Overview";
    m_pluginTabs.push_back(std::move(overview));

    std::unordered_map<std::wstring, size_t> byPlugin;
    byPlugin.emplace(L"__overview__", 0);

    for (const auto& plugin : plugins)
    {
        PluginTab tab;
        tab.pluginId = plugin.id;
        tab.iconKey = ResolvePluginIconKey(plugin);
        if (m_themePlatform)
        {
            const ThemeIconMapping icon = m_themePlatform->ResolveIconMapping(tab.iconKey, L"\uE943");
            tab.iconGlyph = icon.glyph;
            tab.iconAsset = icon.assetPack.empty() ? L"" : (icon.assetPack + L":" + icon.assetName);
        }
        else
        {
            tab.iconGlyph = L"\uE943";
        }
        tab.title = plugin.displayName.empty() ? plugin.id : plugin.displayName;
        byPlugin.emplace(tab.pluginId, m_pluginTabs.size());
        m_pluginTabs.push_back(std::move(tab));
    }

    for (size_t i = 0; i < m_pages.size(); ++i)
    {
        const auto& page = m_pages[i];
        auto found = byPlugin.find(page.pluginId);
        if (found == byPlugin.end())
        {
            PluginTab tab;
            tab.pluginId = page.pluginId;
            tab.iconKey = (page.pluginId == L"builtin.plugins") ? L"settings.plugins" : L"plugins.generic";
            if (m_themePlatform)
            {
                const ThemeIconMapping icon = m_themePlatform->ResolveIconMapping(tab.iconKey, L"\uE943");
                tab.iconGlyph = icon.glyph;
                tab.iconAsset = icon.assetPack.empty() ? L"" : (icon.assetPack + L":" + icon.assetName);
            }
            else
            {
                tab.iconGlyph = L"\uE943";
            }
            tab.title = ResolvePluginDisplayName(page.pluginId, plugins);
            tab.pageIndexes.push_back(i);
            byPlugin.emplace(tab.pluginId, m_pluginTabs.size());
            m_pluginTabs.push_back(std::move(tab));
            continue;
        }

        m_pluginTabs[found->second].pageIndexes.push_back(i);
    }

    if (m_pluginTabs.empty())
    {
        PluginTab fallback;
        fallback.pluginId = L"__overview__";
        fallback.iconKey = L"settings.overview";
        if (m_themePlatform)
        {
            const ThemeIconMapping icon = m_themePlatform->ResolveIconMapping(fallback.iconKey, L"\uE80F");
            fallback.iconGlyph = icon.glyph;
            fallback.iconAsset = icon.assetPack.empty() ? L"" : (icon.assetPack + L":" + icon.assetName);
        }
        else
        {
            fallback.iconGlyph = L"\uE80F";
        }
        fallback.title = L"Overview";
        if (!m_pages.empty())
        {
            fallback.pageIndexes.push_back(0);
        }
        m_pluginTabs.push_back(std::move(fallback));
    }
}

void SettingsWindow::PopulatePluginTabs()
{
    if (!m_navList)
        return;

    // Save previous selection
    size_t previousSelection = m_navList->GetSelectedIndex();

    std::vector<VirtualNavList::Item> items;
    for (const auto& tab : m_pluginTabs) {
        items.push_back({tab.title, tab.iconGlyph, false, true});
    }
    m_navList->SetItems(items);

    m_navHoverIndex = -1;

    size_t selectionToApply = previousSelection;
    if (selectionToApply >= m_pluginTabs.size()) {
        selectionToApply = m_pluginTabs.empty() ? 0 : (m_pluginTabs.size() - 1);
    }
    m_navList->SetSelectedIndex(selectionToApply);
    m_navList->Invalidate();
    ShowSelectedPluginTab();
}

std::wstring SettingsWindow::BuildSelectedTabContent(size_t tabIndex) const
{
    if (tabIndex >= m_pluginTabs.size())
    {
        return L"";
    }

    const auto& tab = m_pluginTabs[tabIndex];

    if (tab.pluginId == L"__overview__")
    {
        std::wstring text;
        text += L"Overview\r\n\r\n";
        text += BuildGeneralContent(m_plugins);
        text += L"\r\n";
        text += BuildPluginsContent(m_plugins);
        text += L"\r\n";
        text += BuildDiagnosticsContent(m_plugins);
        return text;
    }

    std::wstring text;
    PluginStatusView pluginView;
    bool hasPluginView = false;
    for (const auto& plugin : m_plugins)
    {
        if (plugin.id == tab.pluginId)
        {
            pluginView = plugin;
            hasPluginView = true;
            break;
        }
    }

    if (hasPluginView)
    {
        text += BuildPluginOverviewContent(pluginView);
        text += L"\r\n\r\n";
    }

    if (tab.pageIndexes.empty())
    {
        text += L"No dedicated settings pages are registered for this plugin yet.";
        return text;
    }

    text += L"Pages\r\n\r\n";
    for (size_t i = 0; i < tab.pageIndexes.size(); ++i)
    {
        const size_t pageIndex = tab.pageIndexes[i];
        if (pageIndex >= m_pages.size())
        {
            continue;
        }

        const auto& page = m_pages[pageIndex];
        text += L"[" + page.title + L"]\r\n";
        text += page.content + L"\r\n";

        if (i + 1 < tab.pageIndexes.size())
        {
            text += L"\r\n";
        }
    }

    return text;
}

void SettingsWindow::ShowSelectedPluginTab()
{

    if (!m_navList)
        return;
    size_t tabIndex = m_navList->GetSelectedIndex();
    if (tabIndex >= m_pluginTabs.size())
        return;

    if (!m_pageView)
    {
        return;
    }

    // Persist in-flight text edits before rebuilding the right pane.
    CommitPendingTextFieldEdits();

    UpdateShellHeaderAndStatus(tabIndex);
    const bool showPluginTree = ShouldShowPluginTree();

    // Check whether any page in this tab declares interactive fields.
    const auto& tab = m_pluginTabs[tabIndex];
    bool hasFields = false;
    if (tab.pluginId != L"__overview__")
    {
        for (const size_t pi : tab.pageIndexes)
        {
            if (pi < m_pages.size())
            {
                for (const auto& field : m_pages[pi].fields)
                {
                    if (IsFieldVisibleInBasicMode(field))
                    {
                        hasFields = true;
                        break;
                    }
                }
                if (hasFields)
                {
                    break;
                }
            }
        }
    }

    // Suppress redraws on both the main window and the scroll panel while
    // we tear down old controls and create new ones.  This prevents the
    // half-constructed control tree from being painted, which is what
    // produces the "text glitching over everything" visual artifact.
    // Suppress redraws on the scroll panel while rebuilding controls so
    // the half-constructed control tree is never painted (eliminates glitching).
    if (m_rightScrollPanel)
    {
        SendMessageW(m_rightScrollPanel, WM_SETREDRAW, FALSE, 0);
    }
    ClearFieldControls();

    if (hasFields)
    {
        // Hide the read-only EDIT; render interactive controls instead.
        ShowWindow(m_pageView, SW_HIDE);
        if (m_pluginTreeView)
        {
            ShowWindow(m_pluginTreeView, showPluginTree ? SW_SHOW : SW_HIDE);
            if (showPluginTree)
            {
                PopulatePluginTree(tabIndex);
            }
            else
            {
                ClearPluginTree();
            }
        }
        if (m_rightScrollPanel)
        {
            // Show AFTER controls are populated so nothing flashes.
            ShowWindow(m_rightScrollPanel, SW_HIDE);
        }
        SetWindowTextW(m_pageView, L"");

        RECT clientRect{};
        GetClientRect(m_hwnd, &clientRect);
        const int width = clientRect.right - clientRect.left;
        const int navWidth = m_navAnimating
            ? m_navAnimatedWidth
            : (m_navCollapsed ? kNavCollapsedWidth : kNavExpandedWidth);
        const int margin   = 10;
        const int topArea = TopAreaHeight();
        const int rightX   = navWidth + (margin * 2);
        const int rightY   = margin + topArea;
        const int rightW   = width - navWidth - (margin * 3);

        PopulateFieldControls(tabIndex, rightX, rightY, rightW);

        if (m_rightScrollPanel)
        {
            SendMessageW(m_rightScrollPanel, WM_SETREDRAW, TRUE, 0);
            ShowWindow(m_rightScrollPanel, SW_SHOW);
        }
    }
    else
    {
        // Show the read-only EDIT with text content.
        if (m_pluginTreeView)
        {
            ShowWindow(m_pluginTreeView, SW_HIDE);
            ClearPluginTree();
        }
        if (m_rightScrollPanel)
        {
            SendMessageW(m_rightScrollPanel, WM_SETREDRAW, TRUE, 0);
            ShowWindow(m_rightScrollPanel, SW_HIDE);
        }
        ShowWindow(m_pageView, SW_SHOW);
        SetWindowTextW(m_pageView, BuildSelectedTabContent(tabIndex).c_str());
    }

    ApplyWindowTranslucency();


    // Force an immediate, complete repaint of whichever pane is now active.
    if (hasFields && m_rightScrollPanel)
    {
        RedrawWindow(m_rightScrollPanel, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN);
    }
    else if (m_pageView)
    {
        RedrawWindow(m_pageView, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
    }

}

std::vector<SettingsWindow::UiPage> SettingsWindow::BuildPages(
    const std::vector<SettingsPageView>& pages,
    const std::vector<PluginStatusView>& plugins) const
{
    std::vector<UiPage> uiPages;
    uiPages.reserve(pages.size() + plugins.size());

    std::unordered_map<std::wstring, bool> hasSettingsPage;
    hasSettingsPage.reserve(plugins.size());
    for (const auto& plugin : plugins)
    {
        hasSettingsPage[plugin.id] = false;
    }

    for (const auto& page : pages)
    {
        UiPage uiPage;
        uiPage.pageId = page.pageId;
        uiPage.pluginId = page.pluginId;
        uiPage.title = page.title;
        hasSettingsPage[page.pluginId] = true;

        if (page.pageId == L"general")
        {
            uiPage.content = BuildGeneralContent(plugins);
        }
        else if (page.pageId == L"plugins")
        {
            uiPage.content = BuildPluginsContent(plugins);
        }
        else if (page.pageId == L"marketplace.discover")
        {
            uiPage.content = BuildMarketplaceDiscoverContent();
        }
        else if (page.pageId == L"marketplace.installed")
        {
            uiPage.content = BuildMarketplaceInstalledContent(plugins);
        }
        else if (page.pageId == L"marketplace.updates")
        {
            uiPage.content = BuildMarketplaceUpdatesContent(plugins);
        }
        else if (page.pageId == L"marketplace.disabled")
        {
            uiPage.content = BuildMarketplaceDisabledContent(plugins);
        }
        else if (page.pageId == L"diagnostics")
        {
            uiPage.content = BuildDiagnosticsContent(plugins);
        }
        else
        {
            uiPage.content = BuildGenericPageContent(page);
        }

        uiPage.fields = page.fields; // copy interactive field schema from the plugin

        uiPages.push_back(std::move(uiPage));
    }

    for (const auto& plugin : plugins)
    {
        auto it = hasSettingsPage.find(plugin.id);
        const bool alreadyHasPage = (it != hasSettingsPage.end()) ? it->second : false;
        if (alreadyHasPage)
        {
            continue;
        }

        UiPage uiPage;
        uiPage.pageId = L"auto.plugin.overview";
        uiPage.pluginId = plugin.id;
        uiPage.title = L"Overview";
        uiPage.content = BuildPluginOverviewContent(plugin);
        uiPages.push_back(std::move(uiPage));
    }

    if (uiPages.empty())
    {
        uiPages.push_back(UiPage{L"general", L"builtin.settings", L"General", BuildGeneralContent(plugins)});
        uiPages.push_back(UiPage{L"plugins", L"builtin.settings", L"Plugins", BuildPluginsContent(plugins)});
        uiPages.push_back(UiPage{L"diagnostics", L"builtin.settings", L"Diagnostics", BuildDiagnosticsContent(plugins)});
    }

    return uiPages;
}

std::wstring SettingsWindow::BuildGeneralContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"General\r\n\r\n";
    text += L"Everyday app settings live here.\r\n";
    text += L"Plugin-specific options are grouped under each plugin tab.\r\n\r\n";
    text += L"Loaded plugin count: ";

    int loaded = 0;
    for (const auto& plugin : plugins)
    {
        if (plugin.enabled && plugin.loaded)
        {
            ++loaded;
        }
    }

    text += std::to_wstring(loaded);
    text += L" / ";
    text += std::to_wstring(plugins.size());
    text += L"\r\n";
    return text;
}

std::wstring SettingsWindow::BuildPluginsContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Plugin Manager\r\n\r\n";
    text += L"Use Marketplace tabs to discover, install, and update plugins.\r\n\r\n";

    const std::wstring statusFilter = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.manager_filter_status", L"all")
        : L"all";
    const std::wstring textFilter = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.manager_filter_text", L"")
        : L"";

    text += L"Active filter: status='" + statusFilter + L"'";
    if (!textFilter.empty())
    {
        text += L", text='" + textFilter + L"'";
    }
    text += L"\r\n\r\n";

    int loadedCount = 0;
    int failedCount = 0;
    int disabledCount = 0;
    int incompatibleCount = 0;
    for (const auto& plugin : plugins)
    {
        if (!plugin.enabled)
        {
            ++disabledCount;
        }
        else if (plugin.loaded)
        {
            ++loadedCount;
        }
        else
        {
            ++failedCount;
        }

        if (plugin.compatibilityStatus == L"incompatible" || plugin.compatibilityStatus == L"rejected")
        {
            ++incompatibleCount;
        }
    }

    text += L"Summary: ";
    text += L"Loaded=" + std::to_wstring(loadedCount);
    text += L", Failed=" + std::to_wstring(failedCount);
    text += L", Disabled=" + std::to_wstring(disabledCount);
    text += L", Incompatible=" + std::to_wstring(incompatibleCount) + L"\r\n\r\n";

    if (plugins.empty())
    {
        text += L"No plugins are currently registered.\r\n";
        text += L"Try installing from Marketplace - Discover.\r\n";
        return text;
    }

    int shown = 0;
    for (const auto& plugin : plugins)
    {
        if (!MatchesPluginStatusFilter(plugin, statusFilter) || !MatchesPluginTextFilter(plugin, textFilter))
        {
            continue;
        }

        ++shown;
        text += PluginStateBadge(plugin) + L" " + plugin.displayName + L" (" + plugin.id + L", v" + plugin.version + L")\r\n";
        text += L"State: " + PluginStateText(plugin) + L"\r\n";
        std::wstring startupOverride = L"inherit manifest default";
        if (m_settingsRegistry)
        {
            const std::wstring overrideKey = L"settings.plugins.enable." + plugin.id;
            const std::wstring overrideValue = m_settingsRegistry->GetValue(overrideKey, L"");
            if (overrideValue == L"true")
            {
                startupOverride = L"force enabled";
            }
            else if (overrideValue == L"false")
            {
                startupOverride = L"force disabled";
            }
        }
        text += L"Startup override: " + startupOverride + L"\r\n";
        text += L"Compatibility: " + (plugin.compatibilityStatus.empty() ? L"unknown" : plugin.compatibilityStatus) + L"\r\n";
        if (!plugin.compatibilityReason.empty())
        {
            text += L"Compatibility reason: " + plugin.compatibilityReason + L"\r\n";
        }
        text += L"Capabilities: " + JoinCapabilities(plugin.capabilities) + L"\r\n";
        text += L"Manager actions: [scaffold-only]\r\n";
        if (!plugin.lastError.empty())
        {
            text += L"Error: " + plugin.lastError + L"\r\n";
        }
        text += L"\r\n";
    }

    if (shown == 0)
    {
        text += L"No plugins matched the active filter.\r\n";
        text += L"Try setting status filter to 'all' and clearing search text.\r\n";
    }

    return text;
}

std::wstring SettingsWindow::BuildMarketplaceDiscoverContent() const
{
    std::wstring text;
    text += L"Marketplace - Discover\r\n\r\n";

    const bool enabled = !m_settingsRegistry ||
        m_settingsRegistry->GetValue(L"settings.plugins.marketplace_enabled", L"true") == L"true";
    if (!enabled)
    {
        text += L"Marketplace is currently disabled. Enable it in App and Updates settings.\r\n";
        return text;
    }

    const std::wstring query = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.search_query", L"")
        : L"";
    const std::wstring category = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.category_filter", L"all")
        : L"all";
    const bool showIncompatible = m_settingsRegistry
        ? (m_settingsRegistry->GetValue(L"settings.plugins.show_incompatible", L"false") == L"true")
        : false;

    PluginCatalog::CatalogFetcher fetcher;
    std::wstring source;
    std::wstring fetchError;
    if (!TryFetchCatalogWithFallback(m_settingsRegistry, fetcher, source, fetchError))
    {
        text += L"Failed to load plugin catalog.\r\n";
        text += L"Error: " + fetchError + L"\r\n";
        return text;
    }

    text += L"Catalog source: " + source + L"\r\n";
    text += L"Catalog version: " + fetcher.GetCatalog().catalogVersion + L"\r\n";
    text += L"Filter: category='" + category + L"'";
    if (!query.empty())
    {
        text += L", query='" + query + L"'";
    }
    text += L"\r\n\r\n";

    int shown = 0;
    for (const auto& entry : fetcher.GetCatalog().plugins)
    {
        const bool compatible = fetcher.IsPluginCompatible(entry);
        if (!showIncompatible && !compatible)
        {
            continue;
        }
        if (category != L"all" && entry.category != category)
        {
            continue;
        }
        if (!MarketplaceEntryMatchesQuery(entry, query))
        {
            continue;
        }

        ++shown;
        text += entry.displayName + L" (" + entry.id + L")\r\n";
        text += L"Version: " + entry.version + L" | Author: " + entry.author + L" | Channel: " + entry.channel + L"\r\n";
        text += L"Category: " + entry.category + L" | Compatibility: " + (compatible ? L"compatible" : L"incompatible") + L"\r\n";
        text += L"Capabilities: " + JoinCapabilities(entry.capabilities) + L"\r\n";
        if (!entry.description.empty())
        {
            text += L"Description: " + TruncateWithEllipsis(entry.description, 180) + L"\r\n";
        }
        text += L"\r\n";

        if (shown >= 25)
        {
            text += L"Showing first 25 matching plugins. Refine search for more.\r\n";
            break;
        }
    }

    if (shown == 0)
    {
        text += L"No marketplace plugins matched the active filters.\r\n";
    }

    const std::wstring selectedId = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.selected_plugin_id", L"")
        : L"";
    text += L"\r\nSelected plugin id: " + (selectedId.empty() ? L"(none)" : selectedId) + L"\r\n";
    return text;
}

std::wstring SettingsWindow::BuildMarketplaceInstalledContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Marketplace - Installed\r\n\r\n";

    const std::wstring filter = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.installed_filter", L"all")
        : L"all";
    text += L"Installed filter: " + filter + L"\r\n\r\n";

    int shown = 0;
    for (const auto& plugin : plugins)
    {
        const bool requiresRestart = !plugin.enabled || !plugin.loaded;
        if (filter == L"enabled" && !plugin.enabled)
        {
            continue;
        }
        if (filter == L"disabled" && plugin.enabled)
        {
            continue;
        }
        if (filter == L"requires_restart" && !requiresRestart)
        {
            continue;
        }

        ++shown;
        text += plugin.displayName + L" (" + plugin.id + L", v" + plugin.version + L")\r\n";
        text += L"State: " + PluginStateText(plugin) + L"\r\n";
        text += L"Compatibility: " + (plugin.compatibilityStatus.empty() ? L"unknown" : plugin.compatibilityStatus) + L"\r\n\r\n";
    }

    if (shown == 0)
    {
        text += L"No installed plugins matched the active filter.\r\n";
    }

    return text;
}

std::wstring SettingsWindow::BuildMarketplaceUpdatesContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Marketplace - Updates\r\n\r\n";

    PluginCatalog::CatalogFetcher fetcher;
    std::wstring source;
    std::wstring fetchError;
    if (!TryFetchCatalogWithFallback(m_settingsRegistry, fetcher, source, fetchError))
    {
        text += L"Failed to load plugin catalog.\r\n";
        text += L"Error: " + fetchError + L"\r\n";
        return text;
    }

    std::unordered_map<std::wstring, std::wstring> installedVersions;
    for (const auto& plugin : plugins)
    {
        installedVersions[plugin.id] = plugin.version;
    }

    const std::wstring updateScope = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.update_scope", L"selected")
        : L"selected";
    const std::wstring selectedId = m_settingsRegistry
        ? m_settingsRegistry->GetValue(L"settings.plugins.marketplace.selected_plugin_id", L"")
        : L"";

    text += L"Update scope: " + updateScope + L"\r\n";
    if (!selectedId.empty())
    {
        text += L"Selected plugin id: " + selectedId + L"\r\n";
    }
    text += L"\r\n";

    int updates = 0;
    for (const auto& entry : fetcher.GetCatalog().plugins)
    {
        const auto installedIt = installedVersions.find(entry.id);
        if (installedIt == installedVersions.end())
        {
            continue;
        }
        if (updateScope == L"selected" && !selectedId.empty() && entry.id != selectedId)
        {
            continue;
        }
        if (!fetcher.IsPluginCompatible(entry))
        {
            continue;
        }

        const std::wstring installedVersion = installedIt->second;
        if (installedVersion == entry.version)
        {
            continue;
        }

        ++updates;
        text += entry.displayName + L" (" + entry.id + L")\r\n";
        text += L"Installed: " + installedVersion + L" -> Available: " + entry.version + L"\r\n";
        text += L"Restart required: " + std::wstring(entry.restartRequired ? L"yes" : L"no") + L"\r\n\r\n";
    }

    if (updates == 0)
    {
        text += L"No compatible updates are currently available.\r\n";
    }

    return text;
}

std::wstring SettingsWindow::BuildMarketplaceDisabledContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Marketplace - Disabled\r\n\r\n";

    const bool showReason = m_settingsRegistry
        ? (m_settingsRegistry->GetValue(L"settings.plugins.marketplace.show_disabled_reason", L"true") == L"true")
        : true;

    int shown = 0;
    for (const auto& plugin : plugins)
    {
        const bool isDisabled = !plugin.enabled;
        const bool isIncompatible = plugin.compatibilityStatus == L"incompatible" || plugin.compatibilityStatus == L"rejected";
        if (!isDisabled && !isIncompatible)
        {
            continue;
        }

        ++shown;
        text += plugin.displayName + L" (" + plugin.id + L")\r\n";
        text += L"State: " + PluginStateText(plugin) + L"\r\n";
        text += L"Compatibility: " + (plugin.compatibilityStatus.empty() ? L"unknown" : plugin.compatibilityStatus) + L"\r\n";
        if (showReason && !plugin.compatibilityReason.empty())
        {
            text += L"Reason: " + plugin.compatibilityReason + L"\r\n";
        }
        text += L"\r\n";
    }

    if (shown == 0)
    {
        text += L"No disabled or incompatible plugins detected.\r\n";
    }

    return text;
}

std::wstring SettingsWindow::BuildDiagnosticsContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Diagnostics\r\n\r\n";

    int failed = 0;
    int disabled = 0;
    int incompatible = 0;
    int compatible = 0;
    int unknown = 0;
    for (const auto& plugin : plugins)
    {
        if (!plugin.enabled)
        {
            ++disabled;
        }
        else if (!plugin.loaded)
        {
            ++failed;
        }

        if (!plugin.compatibilityStatus.empty() && plugin.compatibilityStatus != L"compatible")
        {
            ++incompatible;
        }
        else if (plugin.compatibilityStatus == L"compatible")
        {
            ++compatible;
        }
        else
        {
            ++unknown;
        }
    }

    text += L"Plugin failures: " + std::to_wstring(failed) + L"\r\n";
    text += L"Plugins disabled: " + std::to_wstring(disabled) + L"\r\n";
    text += L"Compatibility issues: " + std::to_wstring(incompatible) + L"\r\n";
    text += L"Compatibility healthy: " + std::to_wstring(compatible) + L"\r\n";
    text += L"Compatibility unknown: " + std::to_wstring(unknown) + L"\r\n\r\n";

    if (failed > 0 || incompatible > 0)
    {
        text += L"Recommended next steps\r\n";
        text += L"----------------------------------------\r\n";
        text += L"1) Open Plugins tab and inspect entries marked [Failed] or [Incompatible].\r\n";
        text += L"2) Review compatibility reason and last error details.\r\n";
        text += L"3) Use refresh/apply action after fixing plugin state.\r\n\r\n";
    }

    if (m_settingsRegistry)
    {
        const std::wstring lastReloadSummary = m_settingsRegistry->GetValue(L"settings.plugins.last_reload_summary", L"");
        const std::wstring lastReloadUtc = m_settingsRegistry->GetValue(L"settings.plugins.last_reload_utc", L"");
        if (!lastReloadSummary.empty())
        {
            text += L"Last plugin host reload summary: " + lastReloadSummary + L"\r\n\r\n";
        }
        if (!lastReloadUtc.empty())
        {
            text += L"Last plugin host reload UTC: " + lastReloadUtc + L"\r\n\r\n";
        }
    }

    if (plugins.empty())
    {
        text += L"No plugin status records available.\r\n\r\n";
    }
    else
    {
        text += L"Plugin triage summary\r\n";
        text += L"----------------------------------------\r\n";
        for (const auto& plugin : plugins)
        {
            const std::wstring state = PluginStateText(plugin);
            const std::wstring compat = plugin.compatibilityStatus.empty() ? L"unknown" : plugin.compatibilityStatus;

            text += plugin.displayName + L" (" + plugin.id + L")\r\n";
            text += L"  State: " + state + L"\r\n";
            text += L"  Compatibility: " + compat + L"\r\n";
            if (!plugin.compatibilityReason.empty())
            {
                text += L"  Compatibility reason: " + plugin.compatibilityReason + L"\r\n";
            }
            if (!plugin.lastError.empty())
            {
                text += L"  Last error: " + plugin.lastError + L"\r\n";
            }
            text += L"\r\n";
        }
    }

    text += L"This shell can be expanded into a richer plugin manager with actionable controls per plugin.\r\n";
    return text;
}

std::wstring SettingsWindow::BuildGenericPageContent(const SettingsPageView& page) const
{
    if (page.pluginId == L"builtin.core_commands")
    {
        if (page.pageId == L"core.behavior")
        {
            return L"Behavior\r\n\r\n"
                   L"- New space command is routed through kernel command id 'space.create'.\r\n"
                   L"- Space creation remains core-owned for stability.\r\n"
                   L"- Suggested future toggles: default size, default title template, auto-focus on create.";
        }
        if (page.pageId == L"core.provider")
        {
            return L"Content Provider\r\n\r\n"
                   L"- Default provider: core.file_collection\r\n"
                   L"- Fallback normalization is active for unsupported provider ids.\r\n"
                   L"- Suggested future toggles: strict provider mode, migration policy, provider warning verbosity.";
        }
    }

    if (page.pluginId == L"builtin.tray" && page.pageId == L"tray.behavior")
    {
        return L"Tray Behavior\r\n\r\n"
               L"- Tray menu contributions are command-driven.\r\n"
               L"- Current entries: New Space, Settings, Exit.\r\n"
               L"- Suggested future toggles: close-to-tray behavior, startup visibility, menu ordering profile.";
    }

    if ((page.pluginId == L"community.visual_modes" || page.pluginId == L"builtin.appearance") && page.pageId == L"appearance.theme")
    {
        const std::wstring themeText = (m_themeMode == ThemeMode::Dark) ? L"dark" : L"light";
        const std::wstring styleText = (m_themeStyle == ThemeStyle::Win32ThemeCatalog)
            ? L"win32_theme_system"
            : L"legacy";
        return L"Theme\r\n\r\n"
               L"- Theme engine is driven by Win32ThemeSystem with canonical theme IDs.\r\n"
               L"- Active theme: " + themeText + L"\r\n"
               L"- Active style: " + styleText + L"\r\n"
               L"- Palette includes window, surface, nav, text, subtle-text, and accent colors.";
    }

    if (page.pluginId == L"builtin.explorer_portal" && page.pageId == L"explorer.portal")
    {
        return L"Portal Behavior\r\n\r\n"
               L"- Folder portal content provider is registered.\r\n"
               L"- Suggested future toggles: recurse subfolders, show hidden files, pin folder roots.";
    }

    if (page.pluginId == L"builtin.widgets")
    {
        if (page.pageId == L"widgets.layout")
        {
            return L"Widget Layout\r\n\r\n"
                   L"- Widget panel provider is registered.\r\n"
                   L"- Suggested future toggles: compact card mode, default widget density, snap-to-grid.";
        }
        if (page.pageId == L"widgets.refresh")
        {
            return L"Refresh Policy\r\n\r\n"
                   L"- Widget refresh loop is currently placeholder-only.\r\n"
                   L"- Suggested future toggles: refresh interval, pause on battery, refresh-on-focus.";
        }
    }

    if (page.pluginId == L"builtin.desktop_context" && page.pageId == L"desktop.context")
    {
        return L"Context Actions\r\n\r\n"
               L"- Desktop context integration is scaffolded.\r\n"
               L"- Suggested future toggles: show space quick actions, advanced context entries, safety confirmations.";
    }

    std::wstring text;
    text += page.title + L"\r\n\r\n";
    text += L"Plugin id: " + page.pluginId + L"\r\n";
    text += L"Page id: " + page.pageId + L"\r\n\r\n";
    text += L"No dedicated controls yet. This page is currently scaffold-only.";
    return text;
}

std::wstring SettingsWindow::BuildPluginOverviewContent(const PluginStatusView& plugin) const
{
    std::wstring text;
    text += plugin.displayName + L"\r\n\r\n";
    text += L"Plugin id: " + plugin.id + L"\r\n";
    text += L"Version: " + plugin.version + L"\r\n";
    text += L"State: " + PluginStateText(plugin) + L"\r\n";
    text += L"Compatibility: " + (plugin.compatibilityStatus.empty() ? L"unknown" : plugin.compatibilityStatus) + L"\r\n";
    if (!plugin.compatibilityReason.empty())
    {
        text += L"Compatibility reason: " + plugin.compatibilityReason + L"\r\n";
    }
    text += L"Capabilities: " + JoinCapabilities(plugin.capabilities) + L"\r\n";

    if (!plugin.lastError.empty())
    {
        text += L"Last error: " + plugin.lastError + L"\r\n";
    }

    text += L"\r\nThis page was generated automatically because this plugin has no dedicated settings pages registered yet.";
    return text;
}

// ---------------------------------------------------------------------------
// Interactive field controls
// ---------------------------------------------------------------------------

void SettingsWindow::ClearFieldControls()
{
    for (HWND hwnd : m_fieldControls)
    {
        if (hwnd)
        {
            DestroyWindow(hwnd);
        }
    }
    m_fieldControls.clear();
    m_fieldSurfaceTypes.clear();
    m_fieldControlLayouts.clear();
    m_controlFieldMap.clear();
    m_sectionCardRects.clear();
    m_rightPaneTextStatics.clear();
    m_sectionHeaderStatics.clear();
    m_rightPaneScrollY = 0;
    m_rightPaneContentHeight = 0;
    // Reset ID counter so IDs don't climb unboundedly across tab switches.
    m_nextControlId = 2000;
}

bool SettingsWindow::ShouldShowPluginTree() const
{
    if (!m_navList)
    {
        return false;
    }

    if (!m_navList)
        return false;
    size_t activeIdx = m_navList->GetSelectedIndex();
    if (activeIdx >= m_pluginTabs.size())
        return false;
    const auto& tab = m_pluginTabs[activeIdx];
    return tab.pluginId == L"builtin.plugins";
}

void SettingsWindow::ClearPluginTree()
{
    if (!m_pluginTreeView)
    {
        return;
    }
    TreeView_DeleteAllItems(m_pluginTreeView);
}

void SettingsWindow::PopulatePluginTree(size_t tabIndex)
{
    if (!m_pluginTreeView || m_plugins.empty())
    {
        return;
    }

    if (tabIndex >= m_pluginTabs.size() || m_pluginTabs[tabIndex].pluginId != L"builtin.plugins")
    {
        ClearPluginTree();
        return;
    }
    
    ClearPluginTree();
    
    // Populate tree with plugins
    int inserted = 0;
    for (const auto& plugin : m_plugins)
    {
        // Add plugin as root item
        TVINSERTSTRUCT tvi{};
        tvi.hParent = TVI_ROOT;
        tvi.hInsertAfter = TVI_LAST;
        tvi.item.mask = TVIF_TEXT | TVIF_STATE;
        
        std::wstring displayText = PluginStateBadge(plugin) + L" " + plugin.displayName + L" (v" + plugin.version + L")";
        tvi.item.pszText = const_cast<LPWSTR>(displayText.c_str());
        tvi.item.state = TVIS_EXPANDED;
        tvi.item.stateMask = TVIS_EXPANDED;
        
        HTREEITEM hPlugin = TreeView_InsertItem(m_pluginTreeView, &tvi);
        if (!hPlugin)
        {
            continue;
        }

        ++inserted;
        
        // Add status as child item
        TVINSERTSTRUCT tviStatus{};
        tviStatus.hParent = hPlugin;
        tviStatus.hInsertAfter = TVI_LAST;
        tviStatus.item.mask = TVIF_TEXT;
        std::wstring statusText = L"Status: " + (plugin.enabled ? std::wstring(L"Enabled") : std::wstring(L"Disabled"));
        tviStatus.item.pszText = const_cast<LPWSTR>(statusText.c_str());
        TreeView_InsertItem(m_pluginTreeView, &tviStatus);
        
        // Add capabilities as child items if present
        if (!plugin.capabilities.empty())
        {
            TVINSERTSTRUCT tviCap{};
            tviCap.hParent = hPlugin;
            tviCap.hInsertAfter = TVI_LAST;
            tviCap.item.mask = TVIF_TEXT;
            std::wstring capText = L"Capabilities: " + JoinCapabilities(plugin.capabilities);
            tviCap.item.pszText = const_cast<LPWSTR>(capText.c_str());
            TreeView_InsertItem(m_pluginTreeView, &tviCap);
        }

        if (!plugin.compatibilityReason.empty())
        {
            TVINSERTSTRUCT tviReason{};
            tviReason.hParent = hPlugin;
            tviReason.hInsertAfter = TVI_LAST;
            tviReason.item.mask = TVIF_TEXT;
            std::wstring reasonText = L"Reason: " + plugin.compatibilityReason;
            tviReason.item.pszText = const_cast<LPWSTR>(reasonText.c_str());
            TreeView_InsertItem(m_pluginTreeView, &tviReason);
        }
    }

    if (inserted == 0)
    {
        TVINSERTSTRUCT tviEmpty{};
        tviEmpty.hParent = TVI_ROOT;
        tviEmpty.hInsertAfter = TVI_LAST;
        tviEmpty.item.mask = TVIF_TEXT;
        std::wstring emptyText = L"No plugins available.";
        tviEmpty.item.pszText = const_cast<LPWSTR>(emptyText.c_str());
        TreeView_InsertItem(m_pluginTreeView, &tviEmpty);
    }
}

void SettingsWindow::CommitPendingTextFieldEdits()
{
    if (!m_settingsRegistry || !m_rightScrollPanel)
    {
        return;
    }

    for (const auto& [ctrlId, info] : m_controlFieldMap)
    {
        if (info.type != SettingsFieldType::String && info.type != SettingsFieldType::Int)
        {
            continue;
        }

        HWND ctrl = GetDlgItem(m_rightScrollPanel, ctrlId);
        if (!ctrl)
        {
            continue;
        }

        wchar_t buf[1024]{};
        GetWindowTextW(ctrl, buf, static_cast<int>(std::size(buf)));
        m_settingsRegistry->SetValue(info.key, buf);
    }
}

void SettingsWindow::RegisterTooltipForControl(HWND control, const std::wstring& tipText)
{
    if (!m_tooltip || !control || tipText.empty())
    {
        return;
    }

    TOOLINFOW ti{};
    ti.cbSize = sizeof(TOOLINFOW);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = GetParent(control);
    ti.uId = reinterpret_cast<UINT_PTR>(control);
    ti.lpszText = const_cast<LPWSTR>(tipText.c_str());
    GetClientRect(control, &ti.rect);
    SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

void SettingsWindow::UpdateShellHeaderAndStatus(size_t tabIndex)
{
    if (tabIndex >= m_pluginTabs.size())
    {
        return;
    }

    const auto& tab = m_pluginTabs[tabIndex];
    const std::wstring headerText = (tab.pluginId == L"__overview__")
        ? L"Settings Overview"
        : (tab.title.empty() ? L"Settings" : tab.title);

    std::wstring subtitle;
    if (tab.pluginId == L"__overview__")
    {
        subtitle = L"Quick summary of app health and your most important settings.";
    }
    else if (tab.pluginId == L"builtin.settings")
    {
        subtitle = L"Everyday app preferences for behavior, startup, and layout.";
    }
    else if (tab.pluginId == L"builtin.plugins")
    {
        subtitle = L"Browse, install, and manage add-ons in one place.";
    }
    else if (tab.pluginId == L"community.visual_modes")
    {
        subtitle = L"Adjust look and feel, themes, and visual style.";
    }
    else if (tab.pluginId == L"builtin.explorer_portal")
    {
        subtitle = L"Control folder portal behavior and visibility.";
    }
    else
    {
        subtitle = L"Adjust options for this section.";
        if (!tab.pageIndexes.empty())
        {
            subtitle += L" " + std::to_wstring(tab.pageIndexes.size()) + L" page(s) available.";
        }
        else
        {
            subtitle += L" This section currently has overview information only.";
        }
    }

    UpdateMarketplaceStatusState(tabIndex);

    if (m_headerTitle)
    {
        SetWindowTextW(m_headerTitle, headerText.c_str());
    }
    if (m_headerSubtitle)
    {
        subtitle += m_showAdvancedSettings
            ? L"  Advanced mode exposes detailed tuning controls."
            : L"  Basic mode keeps only the most important settings visible.";
        if (!m_marketplaceStatusChip.empty())
        {
            subtitle += L"  [" + m_marketplaceStatusChip + L"]";
        }
        SetWindowTextW(m_headerSubtitle, subtitle.c_str());
    }

    if (m_statusBar)
    {
        int loadedCount = 0;
        int failedCount = 0;
        for (const auto& plugin : m_plugins)
        {
            if (plugin.enabled && plugin.loaded)
            {
                ++loadedCount;
            }
            else if (plugin.enabled && !plugin.loaded)
            {
                ++failedCount;
            }
        }

        std::wstringstream status;
         static const wchar_t kSpinnerFrames[] = { L'|', L'/', L'-', L'\\' };
         const wchar_t spinner = kSpinnerFrames[m_marketplaceSpinnerFrame % 4];
         status << L"Active section: " << (tab.title.empty() ? L"Settings" : tab.title)
             << L"   |   Mode: " << (m_showAdvancedSettings ? L"Advanced" : L"Basic")
             << L"   |   Add-ons running: " << loadedCount << L"/" << m_plugins.size();
         if (failedCount > 0)
         {
             status << L"   |   Needs attention: " << failedCount;
         }
         status
             << (m_marketplaceStatusChip.empty()
                  ? L""
                : (std::wstring(L"   |   Add-on catalog ") + spinner + L" " + m_marketplaceStatusChip))
             << L"   |   Tips: Ctrl+Tab switch sections, F1 help";
        SetWindowTextW(m_statusBar, status.str().c_str());
    }
}

void SettingsWindow::UpdateMarketplaceStatusState(size_t tabIndex)
{
    m_marketplaceSpinnerActive = false;
    m_marketplaceStatusChip.clear();

    if (tabIndex >= m_pluginTabs.size())
    {
        KillTimer(m_hwnd, kMarketplaceSpinnerTimerId);
        return;
    }

    const auto& tab = m_pluginTabs[tabIndex];
    int marketplacePageCount = 0;
    for (const size_t pageIndex : tab.pageIndexes)
    {
        if (pageIndex >= m_pages.size())
        {
            continue;
        }

        const auto& page = m_pages[pageIndex];
        if (StartsWith(page.pageId, L"marketplace."))
        {
            ++marketplacePageCount;
        }
    }

    if (marketplacePageCount > 0)
    {
        m_marketplaceSpinnerActive = true;
        m_marketplaceStatusChip = L"Ready";

        if (m_settingsRegistry)
        {
            const bool enabled = m_settingsRegistry->GetValue(L"settings.plugins.marketplace_enabled", L"true") == L"true";
            m_marketplaceStatusChip = enabled ? L"Live" : L"Disabled";
        }

        SetTimer(m_hwnd, kMarketplaceSpinnerTimerId, 120, nullptr);
        return;
    }

    KillTimer(m_hwnd, kMarketplaceSpinnerTimerId);
}

void SettingsWindow::InvalidateNavTransitionRegion(int oldNavWidth, int newNavWidth)
{
    if (!m_hwnd)
    {
        return;
    }

    RECT client{};
    GetClientRect(m_hwnd, &client);

    const int margin = 10;
    const int topArea = TopAreaHeight();

    const int oldDividerX = oldNavWidth + (margin * 2) - 8;
    const int newDividerX = newNavWidth + (margin * 2) - 8;
    RECT dividerBand{};
    dividerBand.left = (std::max)(0, (std::min)(oldDividerX, newDividerX) - 16);
    dividerBand.top = margin;
    dividerBand.right = (std::min)(client.right, static_cast<LONG>((std::max)(oldDividerX, newDividerX) + 16));
    dividerBand.bottom = (std::max)(dividerBand.top, client.bottom - margin);
    if (dividerBand.right > dividerBand.left && dividerBand.bottom > dividerBand.top)
    {
        RedrawWindow(m_hwnd, &dividerBand, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    }

    const int oldHeaderStart = oldNavWidth + (margin * 2);
    const int newHeaderStart = newNavWidth + (margin * 2);
    RECT headerBand{};
    headerBand.left = (std::max)(0, (std::min)(oldHeaderStart, newHeaderStart) - 12);
    headerBand.top = margin;
    headerBand.right = (std::min)(client.right, static_cast<LONG>((std::max)(oldHeaderStart, newHeaderStart) + 12));
    headerBand.bottom = (std::min)(client.bottom, static_cast<LONG>(margin + topArea));
    if (headerBand.right > headerBand.left && headerBand.bottom > headerBand.top)
    {
        RedrawWindow(m_hwnd, &headerBand, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    }
}

void SettingsWindow::StartNavCollapseAnimation(bool requestedCollapsed)
{
    const int currentWidth = m_navAnimating
        ? m_navAnimatedWidth
        : (m_navCollapsed ? kNavCollapsedWidth : kNavExpandedWidth);
    const int targetWidth = requestedCollapsed ? kNavCollapsedWidth : kNavExpandedWidth;

    if (!m_navAnimating && currentWidth == targetWidth)
    {
        m_navCollapsed = requestedCollapsed;
        return;
    }

    m_pendingNavCollapsed = requestedCollapsed;
    m_navAnimating = true;
    m_navAnimationStartWidth = currentWidth;
    m_navAnimationTargetWidth = targetWidth;
    m_navAnimatedWidth = currentWidth;
    m_navAnimationStartTick = GetTickCount();

    bool reducedMotion = false;
    if (m_settingsRegistry)
    {
        reducedMotion = (m_settingsRegistry->GetValue(L"settings.accessibility.reduced_motion", L"false") == L"true");
    }

    m_navAnimationDurationMs = reducedMotion ? 1 : 180;
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            m_navAnimationDurationMs = resolver->GetMotionDurationMs(resolver->GetSelectedMotionPreset(), 180);
        }
    }
    if (reducedMotion)
    {
        m_navAnimationDurationMs = 1;
    }
    if (m_navAnimationDurationMs <= 0)
    {
        m_navAnimationDurationMs = 1;
    }

    if (m_navAnimationDurationMs <= 1)
    {
        m_navAnimating = false;
        m_navCollapsed = requestedCollapsed;
        m_navAnimatedWidth = targetWidth;
        KillTimer(m_hwnd, kNavAnimationTimerId);

        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
        InvalidateNavTransitionRegion(currentWidth, targetWidth);
        PopulatePluginTabs();
        ShowSelectedPluginTab();
        return;
    }

    SetTimer(m_hwnd, kNavAnimationTimerId, 16, nullptr);
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    InvalidateNavTransitionRegion(currentWidth, m_navAnimatedWidth);
}

void SettingsWindow::StepNavCollapseAnimation()
{
    if (!m_navAnimating)
    {
        KillTimer(m_hwnd, kNavAnimationTimerId);
        return;
    }

    const DWORD now = GetTickCount();
    const DWORD elapsed = now - m_navAnimationStartTick;
    const double t = (m_navAnimationDurationMs <= 0)
        ? 1.0
        : (std::min)(1.0, static_cast<double>(elapsed) / static_cast<double>(m_navAnimationDurationMs));
    const double easedT = t * t * (3.0 - (2.0 * t));
    const int oldWidth = m_navAnimatedWidth;
    const int delta = m_navAnimationTargetWidth - m_navAnimationStartWidth;
    m_navAnimatedWidth = m_navAnimationStartWidth + static_cast<int>(delta * easedT);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    InvalidateNavTransitionRegion(oldWidth, m_navAnimatedWidth);

    if (t >= 1.0)
    {
        const int animatedEndWidth = m_navAnimatedWidth;
        m_navAnimating = false;
        m_navCollapsed = m_pendingNavCollapsed;
        m_navAnimatedWidth = m_navAnimationTargetWidth;
        KillTimer(m_hwnd, kNavAnimationTimerId);
        InvalidateNavTransitionRegion(animatedEndWidth, m_navAnimatedWidth);
        PopulatePluginTabs();
        ShowSelectedPluginTab();
    }
}


std::wstring SettingsWindow::BuildTabHelpText(size_t tabIndex) const
{
    if (tabIndex >= m_pluginTabs.size())
    {
        return L"Settings help is not available for this view.";
    }

    const auto& tab = m_pluginTabs[tabIndex];
    std::wstring text;
    text += tab.title + L"\r\n\r\n";
    text += L"Keyboard hints\r\n";
    text += L"- Ctrl+Tab: move to the next settings area\r\n";
    text += L"- Ctrl+Shift+Tab: move to the previous settings area\r\n";
    text += L"- Tab / Shift+Tab: move through controls\r\n\r\n";
    text += L"Section cards group related options to improve scanning and reduce clutter.\r\n";

    if (tab.pluginId == L"__overview__")
    {
        text += L"\r\nOverview combines global status, plugin health, and diagnostics summaries.";
    }
    else if (!tab.pageIndexes.empty())
    {
        text += L"\r\nThis area contains ";
        text += std::to_wstring(tab.pageIndexes.size());
        text += L" settings section(s).";
    }

    return text;
}

bool SettingsWindow::SelectTabByPluginId(const std::wstring& pluginId)
{
    if (!m_navList)
    {
        return false;
    }

    for (size_t i = 0; i < m_pluginTabs.size(); ++i)
    {
        if (m_pluginTabs[i].pluginId == pluginId)
        {
            m_navList->SetSelectedIndex(i);
            ShowSelectedPluginTab();
            return true;
        }
    }

    return false;
}

HWND SettingsWindow::FindFieldControlByKey(const std::wstring& key) const
{
    if (!m_rightScrollPanel)
    {
        return nullptr;
    }

    for (const auto& [ctrlId, info] : m_controlFieldMap)
    {
        if (info.key != key)
        {
            continue;
        }

        HWND ctrl = GetDlgItem(m_rightScrollPanel, ctrlId);
        if (ctrl)
        {
            return ctrl;
        }
    }

    return nullptr;
}

void SettingsWindow::FocusPreferredSearchField()
{
    if (m_contentSearchEdit && IsWindowVisible(m_contentSearchEdit) && IsWindowEnabled(m_contentSearchEdit))
    {
        SetFocus(m_contentSearchEdit);
        SendMessageW(m_contentSearchEdit, EM_SETSEL, 0, -1);
        return;
    }

    const std::wstring preferredKeys[] = {
        L"settings.plugins.manager_filter_text",
        L"settings.plugins.marketplace.search_query",
        L"settings.plugins.marketplace.selected_plugin_id"
    };

    for (const auto& key : preferredKeys)
    {
        HWND ctrl = FindFieldControlByKey(key);
        if (!ctrl || !IsWindowVisible(ctrl) || !IsWindowEnabled(ctrl))
        {
            continue;
        }

        SetFocus(ctrl);
        wchar_t className[32]{};
        GetClassNameW(ctrl, className, static_cast<int>(std::size(className)));
        if (_wcsicmp(className, L"Edit") == 0)
        {
            SendMessageW(ctrl, EM_SETSEL, 0, -1);
        }
        return;
    }

    if (m_navList)
    {
        SetFocus(m_navList->GetHwnd());
    }
}

bool SettingsWindow::MatchesContentSearch(const SettingsFieldDescriptor& field) const
{
    if (m_contentSearchQuery.empty())
    {
        return true;
    }

    const std::wstring query = ToLowerCopy(m_contentSearchQuery);
    if (ToLowerCopy(field.label).find(query) != std::wstring::npos ||
        ToLowerCopy(field.description).find(query) != std::wstring::npos ||
        ToLowerCopy(field.key).find(query) != std::wstring::npos)
    {
        return true;
    }

    for (const auto& option : field.options)
    {
        if (ToLowerCopy(option.label).find(query) != std::wstring::npos ||
            ToLowerCopy(option.value).find(query) != std::wstring::npos)
        {
            return true;
        }
    }

    return false;
}

bool SettingsWindow::MatchesContentChipFilter(const SettingsFieldDescriptor& field) const
{
    switch (m_activeContentChip)
    {
    case 1:
        return field.type == SettingsFieldType::Bool;
    case 2:
        return field.type == SettingsFieldType::Enum;
    case 3:
        return field.type == SettingsFieldType::String || field.type == SettingsFieldType::Int;
    default:
        return true;
    }
}

bool SettingsWindow::IsBuiltinPluginsTabSelected() const
{
    if (!m_navList)
    {
        return false;
    }

    const size_t tabIndex = m_navList->GetSelectedIndex();
    return tabIndex < m_pluginTabs.size() && m_pluginTabs[tabIndex].pluginId == L"builtin.plugins";
}

bool SettingsWindow::ShouldShowMarketplacePage(const std::wstring& pageId) const
{
    if (!StartsWith(pageId, L"marketplace."))
    {
        return true;
    }

    if (m_marketplaceSubTab == 0)
    {
        return pageId == L"marketplace.discover";
    }

    return pageId == L"marketplace.installed" ||
           pageId == L"marketplace.updates" ||
           pageId == L"marketplace.disabled";
}

void SettingsWindow::ShowMenuBarPopup(int buttonId)
{
    HWND button = nullptr;
    switch (buttonId)
    {
    case kMenuFileId:
        button = m_menuFileButton;
        break;
    case kMenuEditId:
        button = m_menuEditButton;
        break;
    case kMenuViewId:
        button = m_menuViewButton;
        break;
    case kMenuPluginsId:
        button = m_menuPluginsButton;
        break;
    case kHeaderHelpId:
        button = m_headerHelpButton;
        break;
    default:
        break;
    }

    if (!button)
    {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu)
    {
        return;
    }

    const auto addItem = [&](UINT id, const wchar_t* label) {
        AppendMenuW(menu, MF_STRING, id, label);
    };

    switch (buttonId)
    {
    case kMenuFileId:
        addItem(kMenuCmdSaveSettings, L"Save Settings");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        addItem(kMenuCmdCloseSettings, L"Close Settings");
        break;
    case kMenuEditId:
        addItem(kMenuCmdUndo, L"Undo");
        addItem(kMenuCmdRedo, L"Redo");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        addItem(kMenuCmdFind, L"Find");
        break;
    case kMenuViewId:
        addItem(kMenuCmdToggleSidebar, L"Toggle Sidebar");
        addItem(kMenuCmdReloadCurrent, L"Refresh Current Page");
        break;
    case kMenuPluginsId:
        addItem(kMenuCmdOpenPlugins, L"Open Plugins");
        addItem(kMenuCmdCheckUpdates, L"Open Plugin Updates");
        break;
    case kHeaderHelpId:
        addItem(kMenuCmdKeyboardShortcuts, L"Keyboard Shortcuts");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        addItem(kMenuCmdAbout, L"About Spaces");
        break;
    default:
        DestroyMenu(menu);
        return;
    }

    RECT rc{};
    GetWindowRect(button, &rc);
    SetForegroundWindow(m_hwnd);
    const UINT flags = TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON;
    const int cmd = TrackPopupMenuEx(menu, flags, rc.left, rc.bottom + 2, m_hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd != 0)
    {
        HandleMenuBarCommand(static_cast<UINT>(cmd));
    }

    PostMessageW(m_hwnd, WM_NULL, 0, 0);
}

void SettingsWindow::HandleMenuBarCommand(UINT commandId)
{
    switch (commandId)
    {
    case kMenuCmdSaveSettings:
        CommitPendingTextFieldEdits();
        if (m_statusBar)
        {
            SetWindowTextW(m_statusBar, L"Settings saved.");
        }
        return;
    case kMenuCmdCloseSettings:
        SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
        return;
    case kMenuCmdUndo:
    {
        HWND focus = GetFocus();
        if (focus)
        {
            SendMessageW(focus, WM_UNDO, 0, 0);
        }
        return;
    }
    case kMenuCmdRedo:
    {
        HWND focus = GetFocus();
        if (focus)
        {
            constexpr UINT kEditRedoMessage = 0x0454;
            SendMessageW(focus, kEditRedoMessage, 0, 0);
        }
        return;
    }
    case kMenuCmdFind:
        FocusPreferredSearchField();
        return;
    case kMenuCmdToggleSidebar:
        SendMessageW(m_hwnd, WM_COMMAND,
                     MAKEWPARAM(kNavToggleId, BN_CLICKED),
                     reinterpret_cast<LPARAM>(m_navToggleButton));
        return;
    case kMenuCmdReloadCurrent:
        ShowSelectedPluginTab();
        return;
    case kMenuCmdOpenPlugins:
        SelectTabByPluginId(L"builtin.plugins");
        FocusPreferredSearchField();
        return;
    case kMenuCmdCheckUpdates:
        if (SelectTabByPluginId(L"builtin.plugins"))
        {
            HWND ctrl = FindFieldControlByKey(L"settings.plugins.marketplace.updates_action");
            if (ctrl)
            {
                SetFocus(ctrl);
            }
        }
        return;
    case kMenuCmdKeyboardShortcuts:
    {
        size_t tabIndex = m_navList ? m_navList->GetSelectedIndex() : 0;
        MessageBoxW(m_hwnd, BuildTabHelpText(tabIndex).c_str(), L"Keyboard Shortcuts", MB_OK | MB_ICONINFORMATION);
        return;
    }
    case kMenuCmdAbout:
        MessageBoxW(m_hwnd,
                    L"Spaces\r\n\r\nA native Win32 desktop organizer with plugin-powered settings and theming.",
                    L"About Spaces",
                    MB_OK | MB_ICONINFORMATION);
        return;
    default:
        return;
    }
}

void SettingsWindow::DrawNavItem(const DRAWITEMSTRUCT* drawInfo)
{
    if (!drawInfo || drawInfo->itemID == static_cast<UINT>(-1))
    {
        return;
    }

    const size_t index = static_cast<size_t>(drawInfo->itemID);
    if (index >= m_pluginTabs.size())
    {
        return;
    }

    HDC hdc = drawInfo->hDC;
    RECT rc = drawInfo->rcItem;
    const PluginTab& tab = m_pluginTabs[index];
    const bool selected = (drawInfo->itemState & ODS_SELECTED) != 0;
    const bool hovered = (m_navHoverIndex == static_cast<int>(index));
    const bool cyber = IsCyberTheme(m_windowColor);

    HBRUSH background = CreateSolidBrush(m_navColor);
    FillRect(hdc, &rc, background);
    DeleteObject(background);

    RECT pillRc = rc;

    // Apply component family-aware styling
    std::wstring componentFamily = L"desktop-fluent";
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            componentFamily = resolver->GetSelectedComponentFamily();
        }
    }

    if (cyber)
    {
        // Cyberpunk: sharp rectangular selection — no rounded pills, full-width tech aesthetic
        InflateRect(&pillRc, -4, -3);

        if (selected)
        {
            // Deep selected fill: accent blended into a near-black surface
            const COLORREF selectedFill = BlendColor(m_navColor, m_accentColor, 42);
            HBRUSH fillBrush = CreateSolidBrush(selectedFill);
            FillRect(hdc, &pillRc, fillBrush);
            DeleteObject(fillBrush);

            // Left neon edge beam — full height, 3px wide, solid cyan
            RECT beamRc = pillRc;
            beamRc.right = beamRc.left + 3;
            HBRUSH beamBrush = CreateSolidBrush(m_accentColor);
            FillRect(hdc, &beamRc, beamBrush);
            DeleteObject(beamBrush);

            // Subtle glow line at top and bottom of selected row
            HPEN glowPen = CreatePen(PS_SOLID, 1, BlendColor(m_accentColor, m_navColor, 110));
            HGDIOBJ oldGlowPen = SelectObject(hdc, glowPen);
            MoveToEx(hdc, pillRc.left, pillRc.top, nullptr);
            LineTo(hdc, pillRc.right, pillRc.top);
            MoveToEx(hdc, pillRc.left, pillRc.bottom - 1, nullptr);
            LineTo(hdc, pillRc.right, pillRc.bottom - 1);
            SelectObject(hdc, oldGlowPen);
            DeleteObject(glowPen);
        }
        else if (hovered)
        {
            const COLORREF hoverFill = BlendColor(m_navColor, m_accentColor, 16);
            HBRUSH hoverBrush = CreateSolidBrush(hoverFill);
            FillRect(hdc, &pillRc, hoverBrush);
            DeleteObject(hoverBrush);

            // Dim left edge on hover
            RECT hoverBeam = pillRc;
            hoverBeam.right = hoverBeam.left + 2;
            HBRUSH dimBeam = CreateSolidBrush(BlendColor(m_navColor, m_accentColor, 80));
            FillRect(hdc, &hoverBeam, dimBeam);
            DeleteObject(dimBeam);
        }
    }
    else
    {
        // Standard fluent rendering
        int pillPadding = -6;
        if (componentFamily == L"flat-minimal")       pillPadding = -4;
        else if (componentFamily == L"soft-mobile")   pillPadding = -8;

        pillRc = rc;
        InflateRect(&pillRc, pillPadding, -4);

        if (selected || hovered)
        {
            const COLORREF fillColor = selected
                ? BlendColor(m_navColor, m_accentColor, 76)
                : BlendColor(m_navColor, m_accentColor, 28);
            HBRUSH fillBrush = CreateSolidBrush(fillColor);
            FillRect(hdc, &pillRc, fillBrush);
            DeleteObject(fillBrush);
        }

        if (selected)
        {
            RECT accentRc = pillRc;
            int accentWidth = 4;
            if (componentFamily == L"dashboard-modern")  accentWidth = 6;
            else if (componentFamily == L"cyber-futuristic") accentWidth = 3;

            accentRc.right = accentRc.left + accentWidth;
            HBRUSH accentBrush = CreateSolidBrush(m_accentColor);
            FillRect(hdc, &accentRc, accentBrush);
            DeleteObject(accentBrush);
        }
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_textColor);

    const COLORREF iconChipFill = selected
        ? (cyber ? BlendColor(m_accentColor, RGB(0, 0, 0), 60)
                 : BlendColor(m_accentColor, RGB(255, 255, 255), 28))
        : BlendColor(m_surfaceColor, m_textColor, hovered ? 30 : 18);
    const COLORREF iconColor = selected
        ? (cyber ? m_accentColor : RGB(255, 255, 255))
        : BlendColor(m_textColor, m_accentColor, hovered ? 42 : 24);

    RECT iconChipRc = pillRc;
    const int iconBoxSize = m_navCollapsed ? 30 : 28;
    if (m_navCollapsed)
    {
        iconChipRc.left = pillRc.left + (((pillRc.right - pillRc.left) - iconBoxSize) / 2);
    }
    else
    {
        iconChipRc.left = pillRc.left + (cyber ? 14 : 10);
    }
    iconChipRc.top = pillRc.top + (((pillRc.bottom - pillRc.top) - iconBoxSize) / 2);
    iconChipRc.right = iconChipRc.left + iconBoxSize;
    iconChipRc.bottom = iconChipRc.top + iconBoxSize;

    if (!cyber)
    {
        HBRUSH iconChipBrush = CreateSolidBrush(iconChipFill);
        HPEN iconChipPen = CreatePen(PS_SOLID, 1, BlendColor(iconChipFill, m_textColor, 20));
        HGDIOBJ oldPen = SelectObject(hdc, iconChipPen);
        HGDIOBJ oldBrush = SelectObject(hdc, iconChipBrush);
        RoundRect(hdc, iconChipRc.left, iconChipRc.top, iconChipRc.right, iconChipRc.bottom, 8, 8);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(iconChipBrush);
        DeleteObject(iconChipPen);
    }

    if (m_navCollapsed)
    {
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_iconFont ? m_iconFont : (m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT))));
        SetTextColor(hdc, iconColor);
        DrawTextW(hdc, tab.iconGlyph.c_str(), -1, &iconChipRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
    }
    else
    {
        int glyphSpacing = 12;
        if (componentFamily == L"flat-minimal")   glyphSpacing = 10;
        else if (componentFamily == L"soft-mobile") glyphSpacing = 14;
        if (cyber) glyphSpacing = 10;

        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_iconFont ? m_iconFont : (m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT))));
        SetTextColor(hdc, iconColor);
        DrawTextW(hdc, tab.iconGlyph.c_str(), -1, &iconChipRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);

        RECT textRc = pillRc;
        textRc.left = iconChipRc.right + glyphSpacing;
        SelectObject(hdc, m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(hdc, selected
            ? (cyber ? m_accentColor : RGB(255, 255, 255))
            : m_textColor);
        DrawTextW(hdc, tab.title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if ((drawInfo->itemState & ODS_FOCUS) != 0)
    {
        RECT focusRc = pillRc;
        InflateRect(&focusRc, -1, -1);
        HPEN focusPen = CreatePen(PS_SOLID, 2, m_accentColor);
        HGDIOBJ oldFocusPen = SelectObject(hdc, focusPen);
        HGDIOBJ oldFocusBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        if (cyber)
            Rectangle(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom);
        else
            RoundRect(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom, 8, 8);
        SelectObject(hdc, oldFocusBrush);
        SelectObject(hdc, oldFocusPen);
        DeleteObject(focusPen);
    }
}

LRESULT CALLBACK SettingsWindow::NavListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR subclassId, DWORD_PTR refData)
{
    (void)subclassId;

    auto* self = reinterpret_cast<SettingsWindow*>(refData);
    if (!self)
    {
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);

        const POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const LRESULT hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
        const int hoverIndex = (HIWORD(hit) != 0) ? -1 : static_cast<int>(LOWORD(hit));
        const int previousHover = self->m_navHoverIndex;
        if (hoverIndex != self->m_navHoverIndex)
        {
            self->m_navHoverIndex = hoverIndex;

            auto invalidateNavItem = [hwnd](int index)
            {
                if (index < 0)
                {
                    return;
                }

                RECT itemRect{};
                const LRESULT rectResult = SendMessageW(hwnd, LB_GETITEMRECT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&itemRect));
                if (rectResult == LB_ERR)
                {
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }

                InvalidateRect(hwnd, &itemRect, FALSE);
            };

            invalidateNavItem(previousHover);
            invalidateNavItem(hoverIndex);
        }
        break;
    }
    case WM_MOUSELEAVE:
        if (self->m_navHoverIndex != -1)
        {
            RECT itemRect{};
            const int hoverIndex = self->m_navHoverIndex;
            self->m_navHoverIndex = -1;
            const LRESULT rectResult = SendMessageW(hwnd, LB_GETITEMRECT, static_cast<WPARAM>(hoverIndex), reinterpret_cast<LPARAM>(&itemRect));
            if (rectResult == LB_ERR)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else
            {
                InvalidateRect(hwnd, &itemRect, FALSE);
            }
        }
        break;
    default:
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::ScrollPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_ERASEBKGND:
        if (self)
        {
            return 1;
        }
        break;
    case WM_PAINT:
        if (self)
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            const LRESULT result = self->DrawScrollPanelBkgnd(hdc);
            EndPaint(hwnd, &ps);
            return result;
        }
        break;
    case WM_MOUSEWHEEL:
        if (self)
        {
            HWND focus = GetFocus();
            if (focus)
            {
                wchar_t className[32]{};
                GetClassNameW(focus, className, static_cast<int>(std::size(className)));
                if (_wcsicmp(className, L"ComboBox") == 0)
                {
                    const LRESULT dropped = SendMessageW(focus, CB_GETDROPPEDSTATE, 0, 0);
                    if (dropped != 0)
                    {
                        return DefWindowProcW(hwnd, msg, wParam, lParam);
                    }
                }
            }

            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            self->ApplyScrollPanelScroll(delta);
            return 0;
        }
        break;
    case WM_VSCROLL:
        if (self)
        {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);

            int newPos = si.nPos;
            switch (LOWORD(wParam))
            {
            case SB_LINEUP:
                newPos -= 32;
                break;
            case SB_LINEDOWN:
                newPos += 32;
                break;
            case SB_PAGEUP:
                newPos -= static_cast<int>(si.nPage);
                break;
            case SB_PAGEDOWN:
                newPos += static_cast<int>(si.nPage);
                break;
            case SB_THUMBPOSITION:
                newPos = HIWORD(wParam);
                break;
            case SB_THUMBTRACK:
                newPos = si.nTrackPos;
                break;
            default:
                break;
            }

            const int maxPos = (std::max)(0, si.nMax - static_cast<int>(si.nPage) + 1);
            newPos = (std::max)(0, (std::min)(newPos, maxPos));
            if (newPos != self->m_rightPaneScrollY)
            {
                self->m_rightPaneScrollY = newPos;
                self->RelayoutScrollPanelChildren();
            }
            return 0;
        }
        break;
    case WM_COMMAND:
        if (self && self->m_hwnd)
        {
            SendMessageW(self->m_hwnd, WM_COMMAND, wParam, lParam);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    {
        if (!self)
        {
            break;
        }
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);

        if (self->m_rightPaneTextStatics.find(ctrl) != self->m_rightPaneTextStatics.end())
        {
            // Opaque surface fill prevents ghost text trails during repaints.
            const bool cyber = IsCyberTheme(self->m_windowColor);
            if (self->m_sectionHeaderStatics.find(ctrl) != self->m_sectionHeaderStatics.end())
            {
                SetTextColor(hdc, cyber
                    ? self->m_accentColor
                    : BlendColor(self->m_textColor, self->m_accentColor, 72));
            }
            else
            {
                SetTextColor(hdc, self->m_textColor);
            }
            SetBkColor(hdc, self->m_surfaceColor);
            return reinterpret_cast<LRESULT>(self->m_surfaceBrush ? self->m_surfaceBrush : GetStockObject(WHITE_BRUSH));
        }

        SetTextColor(hdc, self->m_textColor);
        SetBkColor(hdc, self->m_surfaceColor);
        return reinterpret_cast<LRESULT>(self->m_surfaceBrush ? self->m_surfaceBrush : GetStockObject(WHITE_BRUSH));
    }
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SettingsWindow::ApplyScrollPanelScroll(int delta)
{
    if (!m_rightScrollPanel)
    {
        return;
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_rightScrollPanel, SB_VERT, &si);

    int newPos = si.nPos - ((delta * 48) / WHEEL_DELTA);
    const int maxPos = (std::max)(0, si.nMax - static_cast<int>(si.nPage) + 1);
    newPos = (std::max)(0, (std::min)(newPos, maxPos));
    if (newPos == m_rightPaneScrollY)
    {
        return;
    }

    m_rightPaneScrollY = newPos;
    RelayoutScrollPanelChildren();
}

void SettingsWindow::RelayoutScrollPanelChildren()
{
    if (!m_rightScrollPanel)
    {
        return;
    }

    RECT panelClient{};
    GetClientRect(m_rightScrollPanel, &panelClient);

    const int panelWidth = static_cast<int>(panelClient.right - panelClient.left);
    const int panelHeight = (std::max)(1, static_cast<int>(panelClient.bottom - panelClient.top));
    const int maxPos = (std::max)(0, m_rightPaneContentHeight - panelHeight);
    m_rightPaneScrollY = (std::max)(0, (std::min)(m_rightPaneScrollY, maxPos));

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (std::max)(0, m_rightPaneContentHeight - 1);
    si.nPage = static_cast<UINT>(panelHeight);
    si.nPos = m_rightPaneScrollY;
    SetScrollInfo(m_rightScrollPanel, SB_VERT, &si, TRUE);

    HDWP defer = BeginDeferWindowPos(static_cast<int>(m_fieldControlLayouts.size()));

    for (const auto& item : m_fieldControlLayouts)
    {
        if (!item.hwnd)
        {
            continue;
        }

        const int width = item.stretchToRight
            ? (std::max)(1, panelWidth - item.baseX - item.rightMargin)
            : item.width;
        const int y = item.baseY - m_rightPaneScrollY;
        const bool isVisible = (y + item.height > 0) && (y < panelHeight);

        if (defer)
        {
            defer = DeferWindowPos(
                defer,
                item.hwnd,
                nullptr,
                item.baseX,
                y,
                width,
                item.height,
                SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_DEFERERASE |
                SWP_NOCOPYBITS |
                (isVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
            continue;
        }

        SetWindowPos(
            item.hwnd,
            nullptr,
            item.baseX,
            y,
            width,
            item.height,
                SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_NOCOPYBITS |
                (isVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }

    if (defer)
    {
        EndDeferWindowPos(defer);
    }

    RedrawWindow(m_rightScrollPanel, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN);
}

LRESULT SettingsWindow::DrawScrollPanelBkgnd(HDC hdc)
{
    if (!m_rightScrollPanel)
    {
        return 0;
    }

    RECT client{};
    GetClientRect(m_rightScrollPanel, &client);

    HBRUSH surfaceBrush = CreateSolidBrush(m_surfaceColor);
    FillRect(hdc, &client, surfaceBrush);
    DeleteObject(surfaceBrush);
    return 1;
}

void SettingsWindow::PopulateFieldControls(size_t tabIndex, int rightX, int rightY, int rightW)
{
    if (!m_rightScrollPanel)
    {
        return;
    }

    RECT panelClient{};
    GetClientRect(m_rightScrollPanel, &panelClient);

    const int panelWidth = panelClient.right - panelClient.left;
    const int panelHeight = panelClient.bottom - panelClient.top;
    (void)rightX;
    (void)rightY;
    (void)rightW;

    m_fieldControlLayouts.clear();
    m_rightPaneScrollY = 0;
    m_rightPaneContentHeight = 0;

    const int leftPad = 12;
    const int topPad = 12;
    const int rightPad = 12;
    const int rightPaneW = (std::max)(120, panelWidth - leftPad - rightPad);

    const HFONT hFont = m_baseFont ? m_baseFont : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HFONT hHeaderFont = m_sectionFont ? m_sectionFont : hFont;
    int         rowH        = 34;
    int         rowGap      = 6;
    int         sectionGap  = 22;
    int         toggleWidth = 62;
    int         toggleHeight = 28;
    const int   labelWidth  = 280;
    const int   ctrlWidth   = 290;
    const int   ctrlGap     = 14;

    if (m_themePlatform)
    {
        rowH = m_themePlatform->GetSettingsRowHeightPx();
        rowGap = m_themePlatform->GetSettingsRowGapPx();
        sectionGap = m_themePlatform->GetSettingsSectionGapPx();
        toggleWidth = m_themePlatform->GetSettingsToggleWidthPx();
        toggleHeight = m_themePlatform->GetSettingsToggleHeightPx();
    }

    int y = topPad;

    const auto& tab = m_pluginTabs[tabIndex];

    for (const size_t pi : tab.pageIndexes)
    {
        if (pi >= m_pages.size())
        {
            continue;
        }
        const auto& page = m_pages[pi];
        if (tab.pluginId == L"builtin.plugins" && !ShouldShowMarketplacePage(page.pageId))
        {
            continue;
        }
        if (page.fields.empty())
        {
            continue;
        }

        std::vector<const SettingsFieldDescriptor*> sorted;
        sorted.reserve(page.fields.size());
        for (const auto& f : page.fields)
        {
            if (IsFieldVisibleInBasicMode(f) && MatchesContentSearch(f) && MatchesContentChipFilter(f))
            {
                sorted.push_back(&f);
            }
        }
        if (sorted.empty())
        {
            continue;
        }

        std::sort(sorted.begin(), sorted.end(),
                  [](const SettingsFieldDescriptor* a, const SettingsFieldDescriptor* b) {
                      return a->order < b->order;
                  });

        // --- Section header (page title) ----------------------------------------
        HWND hHeader = CreateWindowExW(
            0, L"STATIC", page.title.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            leftPad, y, rightPaneW, rowH,
            m_rightScrollPanel, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hHeader)
        {
            SendMessageW(hHeader, WM_SETFONT, reinterpret_cast<WPARAM>(hHeaderFont), TRUE);
            m_fieldControls.push_back(hHeader);
            m_fieldControlLayouts.push_back(FieldControlLayout{hHeader, leftPad, y, rightPaneW, rowH, true, rightPad});
            m_rightPaneTextStatics.insert(hHeader);
            m_sectionHeaderStatics.insert(hHeader);
        }
        y += rowH + 4;

        // --- Fields -------------------------------------------------------------

        for (const SettingsFieldDescriptor* fp : sorted)
        {
            const SettingsFieldDescriptor& field = *fp;

            // Label
            HWND hLabel = CreateWindowExW(
                0, L"STATIC", field.label.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                leftPad, y, labelWidth, rowH,
                m_rightScrollPanel, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (hLabel)
            {
                SendMessageW(hLabel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                m_fieldControls.push_back(hLabel);
                m_fieldControlLayouts.push_back(FieldControlLayout{hLabel, leftPad, y, labelWidth, rowH, false, 0});
                RegisterTooltipForControl(hLabel, field.description);
                m_rightPaneTextStatics.insert(hLabel);
            }

            // Sub-description (optional, shown on the next line)
            if (!field.description.empty())
            {
                // we'll draw the description below the control row later; skip for now
            }

            const int ctrlX  = leftPad + labelWidth + ctrlGap;
            const int ctrlId = m_nextControlId++;
            HWND hCtrl = nullptr;

            if (field.type == SettingsFieldType::Bool)
            {
                const std::wstring curVal = m_settingsRegistry
                    ? m_settingsRegistry->GetValue(field.key, field.defaultValue)
                    : field.defaultValue;
                const bool checked = (curVal == L"true");
                const int normalizedToggleHeight = (std::min)(toggleHeight, rowH);
                const int toggleY = y + ((rowH - normalizedToggleHeight) / 2);

                SwitchControl::Colors switchColors;
                switchColors.surface = m_surfaceColor;
                switchColors.accent = m_accentColor;
                switchColors.text = m_textColor;
                switchColors.window = m_windowColor;

                hCtrl = SwitchControl::Create(
                    m_rightScrollPanel,
                    ctrlX,
                    toggleY,
                    toggleWidth,
                    normalizedToggleHeight,
                    ctrlId,
                    checked,
                    switchColors);
            }
            else if (field.type == SettingsFieldType::Enum)
            {
                hCtrl = CreateWindowExW(
                    0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                    ctrlX, y, ctrlWidth, 200, // height 200 opens dropdown room
                    m_rightScrollPanel,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
                    GetModuleHandleW(nullptr), nullptr);
                if (hCtrl)
                {
                    std::vector<SettingsEnumOption> displayOptions = field.options;
                    if (displayOptions.empty() && m_themePlatform)
                    {
                        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
                        if (resolver)
                        {
                            if (field.key == L"appearance.ui.component_family")
                            {
                                const auto families = resolver->GetAvailableComponentFamilies();
                                for (const auto& familyId : families)
                                {
                                    const std::wstring label = resolver->GetComponentFamilyLabel(familyId);
                                    displayOptions.push_back(SettingsEnumOption{familyId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.icon_pack")
                            {
                                const auto packs = resolver->GetAvailableIconPacks();
                                for (const auto& packId : packs)
                                {
                                    const std::wstring label = resolver->GetIconPackLabel(packId);
                                    displayOptions.push_back(SettingsEnumOption{packId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.motion_preset")
                            {
                                const auto presets = resolver->GetAvailableMotionPresets();
                                for (const auto& presetId : presets)
                                {
                                    const std::wstring label = resolver->GetMotionPresetLabel(presetId);
                                    displayOptions.push_back(SettingsEnumOption{presetId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.button_family")
                            {
                                const auto families = resolver->GetAvailableButtonFamilies();
                                for (const auto& familyId : families)
                                {
                                    const std::wstring label = resolver->GetButtonFamilyLabel(familyId);
                                    displayOptions.push_back(SettingsEnumOption{familyId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.menu_style")
                            {
                                const auto styles = resolver->GetAvailableMenuStyles();
                                for (const auto& styleId : styles)
                                {
                                    const std::wstring label = resolver->GetMenuStyleLabel(styleId);
                                    displayOptions.push_back(SettingsEnumOption{styleId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.fence_style")
                            {
                                const auto styles = resolver->GetAvailableFenceStyles();
                                for (const auto& styleId : styles)
                                {
                                    const std::wstring label = resolver->GetFenceStyleLabel(styleId);
                                    displayOptions.push_back(SettingsEnumOption{styleId, label});
                                }
                            }
                            else if (field.key == L"appearance.ui.controls_family")
                            {
                                const auto families = resolver->GetAvailableControlFamilies();
                                for (const auto& familyId : families)
                                {
                                    const std::wstring label = resolver->GetControlFamilyLabel(familyId);
                                    displayOptions.push_back(SettingsEnumOption{familyId, label});
                                }
                            }
                        }
                    }

                    const std::wstring curVal = m_settingsRegistry
                        ? m_settingsRegistry->GetValue(field.key, field.defaultValue)
                        : field.defaultValue;
                    int selIndex = 0;
                    for (size_t oi = 0; oi < displayOptions.size(); ++oi)
                    {
                        SendMessageW(hCtrl, CB_ADDSTRING, 0,
                                     reinterpret_cast<LPARAM>(displayOptions[oi].label.c_str()));
                        if (displayOptions[oi].value == curVal)
                        {
                            selIndex = static_cast<int>(oi);
                        }
                    }
                    SendMessageW(hCtrl, CB_SETCURSEL, static_cast<WPARAM>(selIndex), 0);

                    // Store options for later - store dynamic options if we populated them, else use field options
                    FieldControlInfo info;
                    info.key = field.key;
                    info.type = field.type;
                    info.options = displayOptions; // use populated or original options
                    m_controlFieldMap[ctrlId] = info;
                }
            }
            else // String or Int
            {
                const std::wstring curVal = m_settingsRegistry
                    ? m_settingsRegistry->GetValue(field.key, field.defaultValue)
                    : field.defaultValue;

                DWORD editStyle = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
                if (field.type == SettingsFieldType::Int)
                {
                    editStyle |= ES_NUMBER;
                }

                hCtrl = CreateWindowExW(
                    0, L"EDIT", curVal.c_str(),
                    editStyle,
                    ctrlX, y, ctrlWidth, rowH,
                    m_rightScrollPanel,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
                    GetModuleHandleW(nullptr), nullptr);
            }

            if (hCtrl)
            {
                SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                m_fieldControls.push_back(hCtrl);
                const int controlTop = (field.type == SettingsFieldType::Bool)
                    ? (y + ((rowH - (std::min)(toggleHeight, rowH)) / 2))
                    : y;
                const int controlHeight = (field.type == SettingsFieldType::Bool)
                    ? (std::min)(toggleHeight, rowH)
                    : rowH;
                m_fieldControlLayouts.push_back(FieldControlLayout{hCtrl, ctrlX, controlTop, ctrlWidth, controlHeight, false, 0});
                RegisterTooltipForControl(hCtrl, field.description);

                if (field.type == SettingsFieldType::Enum ||
                    field.type == SettingsFieldType::String ||
                    field.type == SettingsFieldType::Int)
                {
                    SetWindowSubclass(hCtrl,
                                      &SettingsWindow::FieldSurfaceSubclassProc,
                                      1,
                                      reinterpret_cast<DWORD_PTR>(this));
                    m_fieldSurfaceTypes[hCtrl] = field.type;
                    RedrawWindow(hCtrl, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
                }

                // Don't re-store for Enum types (already stored with dynamic options)
                if (field.type != SettingsFieldType::Enum)
                {
                    FieldControlInfo info;
                    info.key     = field.key;
                    info.type    = field.type;
                    info.options = field.options;
                    m_controlFieldMap.emplace(ctrlId, std::move(info));
                }
            }

            y += rowH + rowGap;
        }

        y += sectionGap;
    }

    m_rightPaneContentHeight = (std::max)(y + topPad, panelHeight);

    if (m_fieldControls.empty())
    {
        HWND hEmpty = CreateWindowExW(
            0, L"STATIC", L"No settings match the current search/filter.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            leftPad, topPad, rightPaneW, rowH,
            m_rightScrollPanel, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hEmpty)
        {
            SendMessageW(hEmpty, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            m_fieldControls.push_back(hEmpty);
            m_fieldControlLayouts.push_back(FieldControlLayout{hEmpty, leftPad, topPad, rightPaneW, rowH, true, rightPad});
            m_rightPaneTextStatics.insert(hEmpty);
            m_rightPaneContentHeight = (std::max)(panelHeight, topPad + rowH + topPad);
        }
    }

    RelayoutScrollPanelChildren();
}

bool SettingsWindow::IsFieldVisibleInBasicMode(const SettingsFieldDescriptor& field) const
{
    const std::wstring& key = field.key;

    // Hide internal/telemetry/test keys in all modes so only actionable settings are shown.
    static const std::unordered_set<std::wstring> kAlwaysHiddenKeys = {
        L"settings.plugins.hub_action",
        L"settings.plugins.manager_action",
        L"settings.plugins.manager_filter_status",
        L"settings.plugins.marketplace.installed_action",
        L"settings.plugins.marketplace.last_check_error",
        L"settings.plugins.marketplace.last_check_fallback",
        L"settings.plugins.marketplace.last_check_source",
        L"settings.plugins.marketplace.last_check_status",
        L"settings.plugins.marketplace.last_check_utc",
        L"test.persistence_marker"
    };

    static const std::vector<std::wstring> kAlwaysHiddenPrefixes = {
        L"test.",
        L"settings.plugins.marketplace.last_check_"
    };

    if (kAlwaysHiddenKeys.find(key) != kAlwaysHiddenKeys.end())
    {
        return false;
    }
    for (const auto& prefix : kAlwaysHiddenPrefixes)
    {
        if (StartsWith(key, prefix))
        {
            return false;
        }
    }

    if (m_showAdvancedSettings)
    {
        return true;
    }

    static const std::unordered_set<std::wstring> kBasicKeys = {
        L"appearance.theme.mode",
        L"theme.win32.theme_id",
        L"appearance.ui.opacity_profile",
        L"appearance.ui.settings_density",
        L"appearance.ui.transparency_enabled",
        L"appearance.ui.icon_size",
        L"spaces.create.size_preset",
        L"spaces.create.auto_focus",
        L"spaces.create.title_template",
        L"spaces.window.close_to_tray",
        L"spaces.window.restore_on_startup",
        L"spaces.window.launch_on_startup",
        L"settings.plugins.marketplace_enabled",
        L"settings.plugins.auto_check_updates",
        L"settings.plugins.update_channel",
        L"explorer.portal.recurse_subfolders",
        L"explorer.portal.show_hidden",
        L"explorer.portal.open_folder_behavior",
        L"organizer.actions.include_hidden",
        L"organizer.actions.skip_shortcuts"
    };

    return kBasicKeys.find(key) != kBasicKeys.end();
}

void SettingsWindow::HandleFieldControlChange(int ctrlId, int notificationCode, HWND hwndCtrl)
{
    if (!m_settingsRegistry || !hwndCtrl)
    {
        return;
    }

    const auto it = m_controlFieldMap.find(ctrlId);
    if (it == m_controlFieldMap.end())
    {
        return;
    }

    const FieldControlInfo& info = it->second;

    auto applyImmediateUiChanges = [&](const std::wstring& changedKey) {
        if (changedKey == L"settings.ui.nav_collapsed" && m_settingsRegistry)
        {
            const bool desiredCollapsed = (m_settingsRegistry->GetValue(L"settings.ui.nav_collapsed", L"false") == L"true");
            m_pendingNavCollapsed = desiredCollapsed;
            PostMessageW(m_hwnd, kMsgApplyNavCollapsed, desiredCollapsed ? 1 : 0, 0);
        }

        if (StartsWith(changedKey, L"appearance.theme.") ||
            changedKey == L"appearance.text.scale_percent" ||
            StartsWith(changedKey, L"appearance.ui.") ||
            StartsWith(changedKey, L"appearance.icons.") ||
            StartsWith(changedKey, L"theme.win32.") ||
            changedKey == L"theme.source")
        {
            RefreshTheme();
        }
    };

    auto refreshCurrentTabForKey = [&](const std::wstring& changedKey) {
        const bool shouldRefresh =
            changedKey == L"settings.plugins.manager_filter_status" ||
            changedKey == L"settings.plugins.manager_filter_text" ||
            changedKey == L"settings.plugins.marketplace.search_query" ||
            changedKey == L"settings.plugins.marketplace.category_filter" ||
            changedKey == L"settings.plugins.marketplace.installed_filter" ||
            changedKey == L"settings.plugins.marketplace.selected_plugin_id" ||
            changedKey == L"settings.plugins.show_incompatible";

        if (shouldRefresh)
        {
            ShowSelectedPluginTab();
        }
    };

    if (info.type == SettingsFieldType::Bool && notificationCode == BN_CLICKED)
    {
        const bool checked = (SendMessageW(hwndCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_settingsRegistry->SetValue(info.key, checked ? L"true" : L"false");
        applyImmediateUiChanges(info.key);
    }
    else if (info.type == SettingsFieldType::Enum && notificationCode == CBN_SELCHANGE)
    {
        const LRESULT sel = SendMessageW(hwndCtrl, CB_GETCURSEL, 0, 0);
        if (sel >= 0 && static_cast<size_t>(sel) < info.options.size())
        {
            const std::wstring selectedValue = info.options[static_cast<size_t>(sel)].value;
            m_settingsRegistry->SetValue(info.key, selectedValue);
            const bool shouldRefreshAfterSelection =
                info.key == L"settings.plugins.manager_filter_status" ||
                info.key == L"settings.plugins.marketplace.category_filter" ||
                info.key == L"settings.plugins.marketplace.installed_filter" ||
                info.key == L"settings.plugins.show_incompatible";

            // Built-in Settings > Plugins action: sync from plugin hub.
            if (info.key == L"settings.plugins.hub_action" && selectedValue == L"sync_now")
            {
                const std::wstring repoUrl = m_settingsRegistry->GetValue(
                    L"settings.plugins.hub_repo_url",
                    L"https://github.com/MrIvoe/Simple-Spaces-Plugins.git");
                const std::wstring branch = m_settingsRegistry->GetValue(
                    L"settings.plugins.hub_branch",
                    L"main");

                const PluginHubSyncResult result = PluginHubSync::SyncFromRepository(repoUrl, branch);
                if (!result.success)
                {
                    Win32Helpers::ShowUserWarning(m_hwnd, L"Plugin Hub Sync Failed", result.message);
                }
                else
                {
                    MessageBoxW(m_hwnd, result.message.c_str(), L"Plugin Hub Sync", MB_OK | MB_ICONINFORMATION);
                }

                // Reset action to idle after running once.
                m_settingsRegistry->SetValue(L"settings.plugins.hub_action", L"idle");
                SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
            }

            if (info.key == L"settings.plugins.manager_action" && selectedValue != L"idle")
            {
                if (selectedValue == L"apply_now")
                {
                    MessageBoxW(
                        m_hwnd,
                        L"Plugin host reload requested. Current enable/disable overrides are being applied now.",
                        L"Plugin Manager",
                        MB_OK | MB_ICONINFORMATION);

                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                    SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
                    ShowSelectedPluginTab();
                    return;
                }

                const std::wstring targetId = m_settingsRegistry->GetValue(L"settings.plugins.manager_target_plugin", L"");

                bool targetFound = false;
                for (const auto& plugin : m_plugins)
                {
                    if (plugin.id == targetId)
                    {
                        targetFound = true;
                        break;
                    }
                }

                if (targetId.empty() || !targetFound)
                {
                    Win32Helpers::ShowUserWarning(
                        m_hwnd,
                        L"Plugin Manager Action",
                        L"Enter a valid plugin id in 'Plugin manager target plugin id' before applying an action.");
                }
                else
                {
                    const std::wstring enableKey = L"settings.plugins.enable." + targetId;
                    if (selectedValue == L"disable_selected")
                    {
                        m_settingsRegistry->SetValue(enableKey, L"false");
                        Win32Helpers::LogInfo(L"Plugin manager override: disabled plugin id='" + targetId + L"' (effective on next plugin host load)");
                    }
                    else if (selectedValue == L"enable_selected")
                    {
                        m_settingsRegistry->SetValue(enableKey, L"true");
                        Win32Helpers::LogInfo(L"Plugin manager override: enabled plugin id='" + targetId + L"' (effective on next plugin host load)");
                    }

                    MessageBoxW(
                        m_hwnd,
                        L"Plugin override saved. The change will take effect on next plugin host load (for example, app restart).",
                        L"Plugin Manager",
                        MB_OK | MB_ICONINFORMATION);
                    ShowSelectedPluginTab();
                }

                m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
            }

            if (info.key == L"settings.plugins.marketplace.action" && selectedValue != L"idle")
            {
                const auto resetEnumAction = [&](const std::wstring& keyToReset) {
                    m_settingsRegistry->SetValue(keyToReset, L"idle");
                    SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
                };

                const auto triggerPluginReload = [&]() {
                    // Re-trigger watcher by toggling through idle.
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"apply_now");
                };

                const auto finishWithApplyDecision = [&](const std::wstring& operationText, bool restartRequired) {
                    const bool applyOnRestart =
                        m_settingsRegistry->GetValue(L"settings.plugins.apply_updates_on_restart", L"true") == L"true";
                    if (restartRequired || applyOnRestart)
                    {
                        m_settingsRegistry->SetValue(L"settings.plugins.marketplace.pending_restart", L"true");
                        m_settingsRegistry->SetValue(L"settings.plugins.marketplace.pending_restart_reason", operationText);
                        MessageBoxW(
                            m_hwnd,
                            (operationText + L"\r\n\r\nThe change is queued for restart-safe apply.").c_str(),
                            L"Marketplace",
                            MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        triggerPluginReload();
                        MessageBoxW(
                            m_hwnd,
                            (operationText + L"\r\n\r\nPlugin host reload requested.").c_str(),
                            L"Marketplace",
                            MB_OK | MB_ICONINFORMATION);
                    }
                    ShowSelectedPluginTab();
                };

                const auto withCatalogEntry = [&](const std::wstring& pluginId,
                                                  const std::function<void(const PluginCatalog::CatalogFetcher&, const PluginCatalog::PluginEntry&)>& action) {
                    PluginCatalog::CatalogFetcher fetcher;
                    std::wstring source;
                    std::wstring fetchError;
                    if (!TryFetchCatalogWithFallback(m_settingsRegistry, fetcher, source, fetchError))
                    {
                        Win32Helpers::ShowUserWarning(
                            m_hwnd,
                            L"Marketplace",
                            L"Failed to load catalog: " + fetchError);
                        return;
                    }

                    const PluginCatalog::PluginEntry* entry = fetcher.FindPlugin(pluginId);
                    if (!entry)
                    {
                        Win32Helpers::ShowUserWarning(
                            m_hwnd,
                            L"Marketplace",
                            L"Selected plugin id was not found in the current catalog.");
                        return;
                    }

                    action(fetcher, *entry);
                };

                const auto installRoot = Win32Helpers::GetAppDataRoot() / L"plugins";
                PluginPackage::PackageInstaller installer(installRoot);
                const bool keepBackup =
                    m_settingsRegistry->GetValue(L"settings.plugins.keep_backup_versions", L"true") == L"true";
                const std::wstring targetId = m_settingsRegistry->GetValue(L"settings.plugins.marketplace.selected_plugin_id", L"");
                const bool marketplaceEnabled =
                    m_settingsRegistry->GetValue(L"settings.plugins.marketplace_enabled", L"true") == L"true";

                auto runSinglePluginAction = [&](const std::wstring& actionValue,
                                                const std::wstring& pluginId) {
                    if (pluginId.empty())
                    {
                        Win32Helpers::ShowUserWarning(
                            m_hwnd,
                            L"Marketplace Action",
                            L"Enter a plugin id in 'Selected plugin id' before running a marketplace action.");
                        return;
                    }

                    if (actionValue == L"enable" || actionValue == L"enable_selected")
                    {
                        m_settingsRegistry->SetValue(L"settings.plugins.enable." + pluginId, L"true");
                        finishWithApplyDecision(L"Plugin enable override saved for '" + pluginId + L"'.", false);
                        return;
                    }

                    if (actionValue == L"disable" || actionValue == L"disable_selected")
                    {
                        m_settingsRegistry->SetValue(L"settings.plugins.enable." + pluginId, L"false");
                        finishWithApplyDecision(L"Plugin disable override saved for '" + pluginId + L"'.", false);
                        return;
                    }

                    if (actionValue == L"uninstall" || actionValue == L"uninstall_selected")
                    {
                        if (!installer.Uninstall(pluginId))
                        {
                            Win32Helpers::ShowUserWarning(
                                m_hwnd,
                                L"Marketplace Uninstall Failed",
                                installer.GetLastError().empty() ? L"Uninstall failed." : installer.GetLastError());
                            return;
                        }

                        // Clear startup override by setting empty value.
                        m_settingsRegistry->SetValue(L"settings.plugins.enable." + pluginId, L"");
                        finishWithApplyDecision(L"Plugin '" + pluginId + L"' was uninstalled.", true);
                        return;
                    }

                    if (actionValue == L"view_details")
                    {
                        withCatalogEntry(pluginId, [&](const PluginCatalog::CatalogFetcher&, const PluginCatalog::PluginEntry& entry) {
                            std::wstring details;
                            details += entry.displayName + L" (" + entry.id + L")\r\n";
                            details += L"Version: " + entry.version + L"\r\n";
                            details += L"Author: " + entry.author + L"\r\n";
                            details += L"Category: " + entry.category + L"\r\n";
                            details += L"Channel: " + entry.channel + L"\r\n";
                            details += L"Restart required: " + std::wstring(entry.restartRequired ? L"yes" : L"no") + L"\r\n";
                            details += L"Capabilities: " + JoinCapabilities(entry.capabilities) + L"\r\n\r\n";
                            details += entry.description.empty() ? L"No description provided." : entry.description;
                            MessageBoxW(m_hwnd, details.c_str(), L"Marketplace Plugin Details", MB_OK | MB_ICONINFORMATION);
                        });
                        return;
                    }

                    if (actionValue == L"install" || actionValue == L"update" || actionValue == L"update_now")
                    {
                        withCatalogEntry(pluginId, [&](const PluginCatalog::CatalogFetcher& fetcher, const PluginCatalog::PluginEntry& entry) {
                            if (!fetcher.IsPluginCompatible(entry))
                            {
                                Win32Helpers::ShowUserWarning(
                                    m_hwnd,
                                    L"Marketplace",
                                    L"Selected plugin is not compatible with this host/API version.");
                                return;
                            }

                            PluginPackage::InstallStatus status = PluginPackage::InstallStatus::InstallFailed;
                            if (actionValue == L"install")
                            {
                                status = installer.InstallFromUrl(entry.id, entry.downloadUrl, entry.hash);
                            }
                            else
                            {
                                status = installer.UpdateFromUrl(entry.id, entry.downloadUrl, entry.hash, keepBackup);
                            }

                            if (status != PluginPackage::InstallStatus::Success)
                            {
                                Win32Helpers::ShowUserWarning(
                                    m_hwnd,
                                    L"Marketplace Package Operation Failed",
                                    installer.GetLastError().empty() ? L"Package operation failed." : installer.GetLastError());
                                return;
                            }

                            const std::wstring opName = (actionValue == L"install") ? L"installed" : L"updated";
                            const std::wstring opText =
                                L"Plugin '" + entry.id + L"' was " + opName +
                                L" from the marketplace package.";
                            finishWithApplyDecision(opText, entry.restartRequired);
                        });
                        return;
                    }

                    Win32Helpers::ShowUserWarning(
                        m_hwnd,
                        L"Marketplace Action",
                        L"Unsupported marketplace action.");
                };

                if (!marketplaceEnabled)
                {
                    Win32Helpers::ShowUserWarning(
                        m_hwnd,
                        L"Marketplace Disabled",
                        L"Enable plugin marketplace in App and Updates before running marketplace actions.");
                    resetEnumAction(L"settings.plugins.marketplace.action");
                    return;
                }

                runSinglePluginAction(selectedValue, targetId);
                resetEnumAction(L"settings.plugins.marketplace.action");
            }

            if (info.key == L"settings.plugins.marketplace.installed_action" && selectedValue != L"idle")
            {
                const std::wstring targetId = m_settingsRegistry->GetValue(L"settings.plugins.marketplace.selected_plugin_id", L"");
                PluginPackage::PackageInstaller installer(Win32Helpers::GetAppDataRoot() / L"plugins");
                if (selectedValue == L"apply_now")
                {
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"apply_now");
                    MessageBoxW(
                        m_hwnd,
                        L"Plugin host reload requested.",
                        L"Marketplace Installed",
                        MB_OK | MB_ICONINFORMATION);
                    ShowSelectedPluginTab();
                }
                else
                {
                    if (targetId.empty())
                    {
                        Win32Helpers::ShowUserWarning(
                            m_hwnd,
                            L"Marketplace Installed",
                            L"Enter a plugin id in 'Selected plugin id' before running installed actions.");
                    }
                    else if (selectedValue == L"enable_selected" || selectedValue == L"disable_selected")
                    {
                        m_settingsRegistry->SetValue(
                            L"settings.plugins.enable." + targetId,
                            selectedValue == L"enable_selected" ? L"true" : L"false");
                        m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                        m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"apply_now");
                        MessageBoxW(
                            m_hwnd,
                            L"Plugin startup override saved. Plugin host reload requested.",
                            L"Marketplace Installed",
                            MB_OK | MB_ICONINFORMATION);
                        ShowSelectedPluginTab();
                    }
                    else if (selectedValue == L"uninstall_selected")
                    {
                        if (!installer.Uninstall(targetId))
                        {
                            Win32Helpers::ShowUserWarning(
                                m_hwnd,
                                L"Marketplace Uninstall Failed",
                                installer.GetLastError().empty() ? L"Uninstall failed." : installer.GetLastError());
                        }
                        else
                        {
                            m_settingsRegistry->SetValue(L"settings.plugins.enable." + targetId, L"");
                            m_settingsRegistry->SetValue(L"settings.plugins.marketplace.pending_restart", L"true");
                            m_settingsRegistry->SetValue(
                                L"settings.plugins.marketplace.pending_restart_reason",
                                L"Plugin '" + targetId + L"' was uninstalled.");
                            MessageBoxW(
                                m_hwnd,
                                L"Plugin was uninstalled. Restart to complete restart-safe apply.",
                                L"Marketplace Installed",
                                MB_OK | MB_ICONINFORMATION);
                            ShowSelectedPluginTab();
                        }
                    }
                }

                m_settingsRegistry->SetValue(L"settings.plugins.marketplace.installed_action", L"idle");
                SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
            }

            if (info.key == L"settings.plugins.marketplace.updates_action" && selectedValue != L"idle")
            {
                const std::wstring updateScope = m_settingsRegistry->GetValue(L"settings.plugins.marketplace.update_scope", L"selected");
                const std::wstring selectedId = m_settingsRegistry->GetValue(L"settings.plugins.marketplace.selected_plugin_id", L"");
                const bool keepBackup =
                    m_settingsRegistry->GetValue(L"settings.plugins.keep_backup_versions", L"true") == L"true";
                const bool applyOnRestart =
                    m_settingsRegistry->GetValue(L"settings.plugins.apply_updates_on_restart", L"true") == L"true";

                if (selectedValue == L"apply_now")
                {
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                    m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"apply_now");
                    MessageBoxW(
                        m_hwnd,
                        L"Plugin host reload requested.",
                        L"Marketplace Updates",
                        MB_OK | MB_ICONINFORMATION);
                    m_settingsRegistry->SetValue(L"settings.plugins.marketplace.updates_action", L"idle");
                    SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
                    ShowSelectedPluginTab();
                    return;
                }

                PluginCatalog::CatalogFetcher fetcher;
                std::wstring source;
                std::wstring fetchError;
                if (!TryFetchCatalogWithFallback(m_settingsRegistry, fetcher, source, fetchError))
                {
                    Win32Helpers::ShowUserWarning(
                        m_hwnd,
                        L"Marketplace Updates",
                        L"Failed to load catalog: " + fetchError);
                    m_settingsRegistry->SetValue(L"settings.plugins.marketplace.updates_action", L"idle");
                    SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
                    return;
                }

                std::unordered_map<std::wstring, std::wstring> installedVersions;
                for (const auto& plugin : m_plugins)
                {
                    installedVersions[plugin.id] = plugin.version;
                }

                PluginPackage::PackageInstaller installer(Win32Helpers::GetAppDataRoot() / L"plugins");
                int updated = 0;
                bool anyRestartRequired = false;
                std::wstring firstError;

                for (const auto& entry : fetcher.GetCatalog().plugins)
                {
                    const auto installedIt = installedVersions.find(entry.id);
                    if (installedIt == installedVersions.end())
                    {
                        continue;
                    }
                    if (updateScope == L"selected" && !selectedId.empty() && entry.id != selectedId)
                    {
                        continue;
                    }
                    if (!fetcher.IsPluginCompatible(entry))
                    {
                        continue;
                    }
                    if (installedIt->second == entry.version)
                    {
                        continue;
                    }

                    const auto status = installer.UpdateFromUrl(entry.id, entry.downloadUrl, entry.hash, keepBackup);
                    if (status != PluginPackage::InstallStatus::Success)
                    {
                        if (firstError.empty())
                        {
                            firstError = installer.GetLastError();
                        }
                        continue;
                    }

                    ++updated;
                    anyRestartRequired = anyRestartRequired || entry.restartRequired;
                }

                if (updated == 0)
                {
                    Win32Helpers::ShowUserWarning(
                        m_hwnd,
                        L"Marketplace Updates",
                        firstError.empty() ? L"No plugin updates were applied." : firstError);
                }
                else
                {
                    if (applyOnRestart || anyRestartRequired)
                    {
                        m_settingsRegistry->SetValue(L"settings.plugins.marketplace.pending_restart", L"true");
                        m_settingsRegistry->SetValue(
                            L"settings.plugins.marketplace.pending_restart_reason",
                            L"Updated " + std::to_wstring(updated) + L" plugin(s). Restart required for safe apply.");
                        MessageBoxW(
                            m_hwnd,
                            (L"Updated " + std::to_wstring(updated) + L" plugin(s).\r\n\r\nChanges are queued for restart-safe apply.").c_str(),
                            L"Marketplace Updates",
                            MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"idle");
                        m_settingsRegistry->SetValue(L"settings.plugins.manager_action", L"apply_now");
                        MessageBoxW(
                            m_hwnd,
                            (L"Updated " + std::to_wstring(updated) + L" plugin(s). Plugin host reload requested.").c_str(),
                            L"Marketplace Updates",
                            MB_OK | MB_ICONINFORMATION);
                    }
                    ShowSelectedPluginTab();
                }

                m_settingsRegistry->SetValue(L"settings.plugins.marketplace.updates_action", L"idle");
                SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
            }

            if (info.key == L"appearance.theme.preset_action" && m_themePlatform)
            {
                std::wstring selectedPath;
                bool completed = false;

                if (selectedValue == L"import")
                {
                    if (Win32Helpers::PromptOpenThemeImportFile(m_hwnd, L"Import Theme Preset or Package", selectedPath))
                    {
                        completed = true;

                        bool imported = false;
                        if (HasZipExtension(selectedPath))
                        {
                            ThemePackageValidator validator;
                            const auto validation = validator.ValidatePackage(selectedPath);
                            if (!validation.isValid)
                            {
                                Win32Helpers::ShowUserWarning(
                                    m_hwnd,
                                    L"Theme Package Validation Failed",
                                    validation.errorMessage.empty() ? L"The selected package is invalid." : validation.errorMessage);
                            }
                            else
                            {
                                ThemePackageLoader loader;
                                const auto loadResult = loader.LoadPackage(selectedPath);
                                if (!loadResult.success)
                                {
                                    Win32Helpers::ShowUserWarning(
                                        m_hwnd,
                                        L"Theme Package Load Failed",
                                        loadResult.errorMessage.empty() ? L"Could not parse theme package." : loadResult.errorMessage);
                                }
                                else
                                {
                                    const auto tempPresetPath = (std::filesystem::temp_directory_path() / L"simplespaces-imported-theme-preset.json").wstring();
                                    if (!BuildPresetJsonFromTokenMap(loadResult.tokenMap, tempPresetPath))
                                    {
                                        Win32Helpers::ShowUserWarning(
                                            m_hwnd,
                                            L"Theme Package Load Failed",
                                            L"Package token map is missing required color tokens.");
                                    }
                                    else if (!m_themePlatform->ImportCustomPreset(tempPresetPath))
                                    {
                                        Win32Helpers::ShowUserWarning(
                                            m_hwnd,
                                            L"Theme Import Failed",
                                            L"The package was loaded but could not be applied.");
                                    }
                                    else
                                    {
                                        imported = true;
                                        if (m_settingsRegistry)
                                        {
                                            m_settingsRegistry->SetValue(L"theme.source", L"win32_theme_system");
                                            m_settingsRegistry->SetValue(L"theme.win32.theme_id", validation.themeId);
                                            m_settingsRegistry->SetValue(L"theme.win32.display_name", validation.displayName);
                                            m_settingsRegistry->SetValue(L"theme.win32.catalog_version", validation.version);
                                        }
                                        Win32Helpers::LogInfo(L"Theme package imported: id='" + validation.themeId + L"' name='" + validation.displayName + L"'");
                                    }

                                    std::error_code ec;
                                    std::filesystem::remove(tempPresetPath, ec);
                                    ThemePackageLoader::CleanupExtraction(loadResult.extractedPath);
                                }
                            }
                        }
                        else if (!m_themePlatform->ImportCustomPreset(selectedPath))
                        {
                            Win32Helpers::ShowUserWarning(m_hwnd, L"Theme Import Failed", L"The selected preset could not be imported.");
                        }
                        else
                        {
                            imported = true;
                            Win32Helpers::LogInfo(L"Theme JSON preset imported: " + selectedPath);
                        }

                        if (!imported)
                        {
                            Win32Helpers::LogInfo(L"Theme import did not apply changes.");
                        }
                    }
                }
                else if (selectedValue == L"export")
                {
                    selectedPath = L"SimpleSpaces-theme.json";
                    if (Win32Helpers::PromptSaveJsonFile(m_hwnd, L"Export Theme Preset", selectedPath))
                    {
                        completed = true;
                        if (!m_themePlatform->ExportCustomPreset(selectedPath))
                        {
                            Win32Helpers::ShowUserWarning(m_hwnd, L"Theme Export Failed", L"The current preset could not be exported.");
                        }
                    }
                }

                if (completed)
                {
                    m_settingsRegistry->SetValue(L"appearance.theme.preset_action", L"idle");
                    SendMessageW(hwndCtrl, CB_SETCURSEL, 0, 0);
                }
            }

            applyImmediateUiChanges(info.key);
            if (shouldRefreshAfterSelection)
            {
                refreshCurrentTabForKey(info.key);
            }
        }
    }
    else if (info.type == SettingsFieldType::String && notificationCode == EN_CHANGE)
    {
        wchar_t buf[1024]{};
        GetWindowTextW(hwndCtrl, buf, static_cast<int>(std::size(buf)));
        const std::wstring value = buf;
        if (m_settingsRegistry->GetValue(info.key, L"") != value)
        {
            m_settingsRegistry->SetValue(info.key, value);
        }
    }
    else if ((info.type == SettingsFieldType::String || info.type == SettingsFieldType::Int)
             && notificationCode == EN_KILLFOCUS)
    {
        wchar_t buf[1024]{};
        GetWindowTextW(hwndCtrl, buf, static_cast<int>(std::size(buf)));
        m_settingsRegistry->SetValue(info.key, buf);
        applyImmediateUiChanges(info.key);
        if (info.type == SettingsFieldType::String)
        {
            refreshCurrentTabForKey(info.key);
        }
    }
}

void SettingsWindow::DrawToggleControl(const DRAWITEMSTRUCT* drawInfo)
{
    if (!drawInfo || !drawInfo->hwndItem)
    {
        return;
    }

    HDC hdc = drawInfo->hDC;
    RECT rc = drawInfo->rcItem;

    const bool checked = (SendMessageW(drawInfo->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
    const bool focused = (drawInfo->itemState & ODS_FOCUS) != 0;
    const bool pressed = (drawInfo->itemState & ODS_SELECTED) != 0;
    const bool disabled = (drawInfo->itemState & ODS_DISABLED) != 0;

    std::wstring controlFamily = L"desktop-fluent";
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            controlFamily = resolver->GetSelectedControlFamily();
        }
    }

    HBRUSH backgroundBrush = CreateSolidBrush(m_surfaceColor);
    FillRect(hdc, &rc, backgroundBrush);
    DeleteObject(backgroundBrush);

    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    int trackWidth = min(50, max(40, width - 4));
    int trackHeight = min(28, max(18, height - 6));
    if (controlFamily == L"flat-minimal")
    {
        trackHeight = min(22, max(16, height - 8));
    }
    else if (controlFamily == L"soft-mobile")
    {
        trackWidth = min(54, max(44, width - 2));
        trackHeight = min(30, max(20, height - 4));
    }

    RECT track{};
    track.left = rc.left + ((width - trackWidth) / 2);
    track.top = rc.top + ((height - trackHeight) / 2);
    track.right = track.left + trackWidth;
    track.bottom = track.top + trackHeight;

    COLORREF trackColor = checked
        ? BlendColor(m_accentColor, RGB(255, 255, 255), pressed ? 14 : 0)
        : BlendColor(m_surfaceColor, m_textColor, 38);
    if (disabled)
    {
        trackColor = BlendColor(m_surfaceColor, trackColor, 110);
    }

    int borderWidth = 1;
    if (controlFamily == L"dashboard-modern")
    {
        borderWidth = 2;
    }
    HPEN borderPen = CreatePen(PS_SOLID, borderWidth, BlendColor(trackColor, m_textColor, 28));
    HBRUSH trackBrush = CreateSolidBrush(trackColor);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, trackBrush);
    int radius = trackHeight;
    if (controlFamily == L"flat-minimal")
    {
        radius = 6;
    }
    RoundRect(hdc, track.left, track.top, track.right, track.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(trackBrush);
    DeleteObject(borderPen);

    const int knobSize = trackHeight - 6;
    int knobLeft = checked ? (track.right - knobSize - 3) : (track.left + 3);
    if (pressed)
    {
        knobLeft += checked ? -1 : 1;
    }

    RECT knob{};
    knob.left = knobLeft;
    knob.top = track.top + 3;
    knob.right = knob.left + knobSize;
    knob.bottom = knob.top + knobSize;

    COLORREF knobColor = disabled
        ? BlendColor(m_surfaceColor, RGB(255, 255, 255), 160)
        : RGB(255, 255, 255);
    HBRUSH knobBrush = CreateSolidBrush(knobColor);
    HPEN knobPen = CreatePen(PS_SOLID, 1, BlendColor(knobColor, RGB(0, 0, 0), 25));
    oldPen = SelectObject(hdc, knobPen);
    oldBrush = SelectObject(hdc, knobBrush);
    Ellipse(hdc, knob.left, knob.top, knob.right, knob.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(knobBrush);
    DeleteObject(knobPen);

    if (focused)
    {
        RECT focusRc = track;
        InflateRect(&focusRc, 2, 2);
        DrawFocusRect(hdc, &focusRc);
    }
}

void SettingsWindow::DrawShellButton(const DRAWITEMSTRUCT* drawInfo)
{
    if (!drawInfo)
    {
        return;
    }

    const bool pressed = (drawInfo->itemState & ODS_SELECTED) != 0;
    const bool focused = (drawInfo->itemState & ODS_FOCUS) != 0;
    const bool hovered = (drawInfo->itemState & ODS_HOTLIGHT) != 0;
    const bool disabled = (drawInfo->itemState & ODS_DISABLED) != 0;
    const bool chipSelected =
        (drawInfo->CtlID == kChipAllId && m_activeContentChip == 0) ||
        (drawInfo->CtlID == kChipToggleId && m_activeContentChip == 1) ||
        (drawInfo->CtlID == kChipChoiceId && m_activeContentChip == 2) ||
        (drawInfo->CtlID == kChipTextId && m_activeContentChip == 3);
    const bool marketplaceTabSelected =
        (drawInfo->CtlID == kMarketplaceDiscoverTabId && m_marketplaceSubTab == 0) ||
        (drawInfo->CtlID == kMarketplaceInstalledTabId && m_marketplaceSubTab == 1);
    const bool modeSelected =
        (drawInfo->CtlID == kModeBasicId && !m_showAdvancedSettings) ||
        (drawInfo->CtlID == kModeAdvancedId && m_showAdvancedSettings);
    const bool isMenuButton =
        drawInfo->CtlID == kMenuFileId ||
        drawInfo->CtlID == kMenuEditId ||
        drawInfo->CtlID == kMenuViewId ||
        drawInfo->CtlID == kMenuPluginsId ||
        drawInfo->CtlID == kHeaderHelpId;
    const bool isNavToggle = (drawInfo->CtlID == kNavToggleId);
    const bool isModeButton = (drawInfo->CtlID == kModeBasicId || drawInfo->CtlID == kModeAdvancedId);
    const bool cyber = IsCyberTheme(m_windowColor);

    std::wstring buttonFamily = L"compact";
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            buttonFamily = resolver->GetSelectedButtonFamily();
        }
    }

    RECT rc = drawInfo->rcItem;
    const int radius = isMenuButton ? 4 : ((buttonFamily == L"soft") ? 9 : 6);
    const int inset = 1;

    COLORREF bg = BlendColor(m_surfaceColor, m_navColor, 90);
    COLORREF border = BlendColor(m_surfaceColor, m_textColor, 44);
    COLORREF text = m_textColor;

    if (isMenuButton)
    {
        bg = hovered || pressed
            ? BlendColor(m_windowColor, m_accentColor, cyber ? 18 : 26)
            : m_windowColor;
        border = hovered || pressed
            ? BlendColor(m_accentColor, m_windowColor, cyber ? 50 : 90)
            : BlendColor(m_windowColor, m_textColor, 18);
        text = hovered || pressed
            ? (cyber ? m_accentColor : RGB(255, 255, 255))
            : m_textColor;
    }

    if (isNavToggle)
    {
        bg = BlendColor(m_navColor, m_accentColor, hovered ? (cyber ? 24 : 18) : (cyber ? 16 : 10));
        border = BlendColor(m_accentColor, m_navColor, cyber ? 48 : 90);
        text = cyber ? m_accentColor : RGB(255, 255, 255);
    }

    if (buttonFamily == L"outlined")
    {
        bg = BlendColor(m_windowColor, m_surfaceColor, hovered ? 24 : 12);
        border = hovered ? m_accentColor : BlendColor(m_surfaceColor, m_textColor, 56);
    }

    if (chipSelected || marketplaceTabSelected || modeSelected)
    {
        bg = BlendColor(m_accentColor, m_surfaceColor, isModeButton ? 56 : 48);
        border = m_accentColor;
        text = cyber ? m_accentColor : RGB(255, 255, 255);
    }
    else if (buttonFamily == L"high-contrast")
    {
        bg = hovered ? m_accentColor : BlendColor(m_surfaceColor, m_textColor, 105);
        border = hovered ? m_accentColor : BlendColor(m_surfaceColor, m_textColor, 120);
        text = hovered ? RGB(255, 255, 255) : m_textColor;
    }
    else if (buttonFamily == L"soft")
    {
        bg = BlendColor(m_surfaceColor, m_accentColor, hovered ? 36 : 20);
        border = BlendColor(bg, m_textColor, 24);
    }

    if (pressed)
    {
        bg = BlendColor(bg, m_textColor, 24);
        border = BlendColor(border, m_textColor, 24);
    }
    if (disabled)
    {
        bg = BlendColor(m_surfaceColor, m_windowColor, 60);
        border = BlendColor(m_surfaceColor, m_textColor, 24);
        text = BlendColor(m_textColor, m_surfaceColor, 120);
    }

    HDC hdc = drawInfo->hDC;
    SetBkMode(hdc, TRANSPARENT);

    RECT buttonRc = rc;
    InflateRect(&buttonRc, -inset, -inset);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH brush = CreateSolidBrush(bg);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    if (cyber && (isMenuButton || isNavToggle || isModeButton))
    {
        Rectangle(hdc, buttonRc.left, buttonRc.top, buttonRc.right, buttonRc.bottom);
    }
    else
    {
        RoundRect(hdc, buttonRc.left, buttonRc.top, buttonRc.right, buttonRc.bottom, radius, radius);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    if (cyber && (isMenuButton || isModeButton) && (hovered || pressed || modeSelected))
    {
        HPEN accentPen = CreatePen(PS_SOLID, 1, m_accentColor);
        HGDIOBJ oldAccentPen = SelectObject(hdc, accentPen);
        MoveToEx(hdc, buttonRc.left + 4, buttonRc.bottom - 2, nullptr);
        LineTo(hdc, buttonRc.right - 4, buttonRc.bottom - 2);
        SelectObject(hdc, oldAccentPen);
        DeleteObject(accentPen);
    }

    wchar_t textBuffer[128]{};
    GetWindowTextW(drawInfo->hwndItem, textBuffer, static_cast<int>(std::size(textBuffer)));
    RECT textRc = buttonRc;
    if (pressed)
    {
        OffsetRect(&textRc, 1, 1);
    }
    SetTextColor(hdc, text);
    SelectObject(hdc, isNavToggle
        ? (m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT))
        : (m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)));
    DrawTextW(hdc, textBuffer, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (focused)
    {
        RECT focusRc = buttonRc;
        InflateRect(&focusRc, -2, -2);
        HPEN focusPen = CreatePen(PS_SOLID, 2, BlendColor(m_accentColor, RGB(255, 255, 255), 28));
        HGDIOBJ oldFocusPen = SelectObject(hdc, focusPen);
        HGDIOBJ oldFocusBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        if (cyber && (isMenuButton || isNavToggle || isModeButton))
            Rectangle(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom);
        else
            RoundRect(hdc, focusRc.left, focusRc.top, focusRc.right, focusRc.bottom, radius, radius);
        SelectObject(hdc, oldFocusBrush);
        SelectObject(hdc, oldFocusPen);
        DeleteObject(focusPen);
    }
}

void SettingsWindow::DrawFieldSurfaceFrame(HWND control)
{
    if (!control)
    {
        return;
    }

    RECT wr{};
    GetWindowRect(control, &wr);
    OffsetRect(&wr, -wr.left, -wr.top);

    HDC hdc = GetWindowDC(control);
    if (!hdc)
    {
        return;
    }

    std::wstring controlFamily = L"desktop-fluent";
    if (m_themePlatform)
    {
        ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
        if (resolver)
        {
            controlFamily = resolver->GetSelectedControlFamily();
        }
    }

    const bool focused = (GetFocus() == control);
    const bool cyber = IsCyberTheme(m_windowColor);
    int borderWidth = 1;
    if (controlFamily == L"dashboard-modern")
    {
        borderWidth = 2;
    }

    COLORREF borderColor = focused
        ? m_accentColor
        : BlendColor(m_surfaceColor, m_textColor, 52);
    COLORREF glowColor = BlendColor(m_accentColor, m_windowColor, 76);

    HPEN pen = CreatePen(PS_SOLID, borderWidth, borderColor);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    const int radius = (controlFamily == L"soft-mobile") ? 8 : 5;
    if (cyber)
    {
        Rectangle(hdc, wr.left, wr.top, wr.right, wr.bottom);
        HPEN accentPen = CreatePen(PS_SOLID, 1, focused ? m_accentColor : glowColor);
        HGDIOBJ oldAccentPen = SelectObject(hdc, accentPen);
        MoveToEx(hdc, wr.left + 1, wr.top + 1, nullptr);
        LineTo(hdc, wr.right - 1, wr.top + 1);
        SelectObject(hdc, oldAccentPen);
        DeleteObject(accentPen);
    }
    else
    {
        Rectangle(hdc, wr.left, wr.top, wr.right, wr.bottom);
        if (radius > 5)
        {
            RoundRect(hdc, wr.left + 1, wr.top + 1, wr.right - 1, wr.bottom - 1, radius, radius);
        }
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    ReleaseDC(control, hdc);
}

LRESULT CALLBACK SettingsWindow::FieldSurfaceSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                          UINT_PTR subclassId, DWORD_PTR refData)
{
    (void)subclassId;
    auto* self = reinterpret_cast<SettingsWindow*>(refData);
    if (!self)
    {
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
    case WM_NCPAINT:
    {
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        self->DrawFieldSurfaceFrame(hwnd);
        return result;
    }
    case WM_PAINT:
    {
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        self->DrawFieldSurfaceFrame(hwnd);
        return result;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        break;
    case WM_NCDESTROY:
        self->m_fieldSurfaceTypes.erase(hwnd);
        RemoveWindowSubclass(hwnd, &SettingsWindow::FieldSurfaceSubclassProc, 1);
        break;
    default:
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow* self = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->WndProc(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == ThemePlatform::GetThemeChangedMessageId())
    {
        RefreshTheme();
        return 0;
    }

    switch (msg)
    {
    case WM_CREATE:
    {
        m_menuFileButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"File",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            56,
            kMenuBarHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMenuFileId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_menuEditButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Edit",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            56,
            kMenuBarHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMenuEditId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_menuViewButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"View",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            56,
            kMenuBarHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMenuViewId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_menuPluginsButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Plugins",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            72,
            kMenuBarHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMenuPluginsId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_contentSearchEdit = CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0,
            0,
            180,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kContentSearchId)),
            GetModuleHandleW(nullptr),
            nullptr);
        if (m_contentSearchEdit)
        {
            SendMessageW(m_contentSearchEdit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search settings"));
        }

        m_chipAllButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"All",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            54,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kChipAllId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_chipToggleButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Switches",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            74,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kChipToggleId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_chipChoiceButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Lists",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            74,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kChipChoiceId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_chipTextButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Text fields",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            64,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kChipTextId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_modeBasicButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Basic",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            64,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kModeBasicId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_modeAdvancedButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Advanced",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            82,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kModeAdvancedId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_marketplaceDiscoverTabButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Discover",
            WS_CHILD | BS_OWNERDRAW,
            0,
            0,
            88,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMarketplaceDiscoverTabId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_marketplaceInstalledTabButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Installed",
            WS_CHILD | BS_OWNERDRAW,
            0,
            0,
            88,
            kSearchRowHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMarketplaceInstalledTabId)),
            GetModuleHandleW(nullptr),
            nullptr);
        
        m_pluginTreeView = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_TREEVIEWW,
            L"",
            WS_CHILD | TVS_LINESATROOT | TVS_HASLINES | TVS_HASBUTTONS | TVS_FULLROWSELECT | WS_VSCROLL,
            0,
            0,
            100,
            100,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPluginTreeViewId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_headerTitle = CreateWindowExW(
            0,
            L"STATIC",
            L"Settings",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0,
            0,
            100,
            20,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHeaderTitleId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_headerSubtitle = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0,
            0,
            100,
            20,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHeaderSubtitleId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_headerHelpButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Help",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            90,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHeaderHelpId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_navToggleButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Hide",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0,
            0,
            28,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNavToggleId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_navList = new VirtualNavList(hwnd, m_themePlatform);
        m_navList->SetOnItemClick([this](size_t) {
            ShowSelectedPluginTab();
        });

        m_pageView = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            0,
            0,
            100,
            100,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPageId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_rightScrollPanel = CreateWindowExW(
            0,
            L"SimpleSpaces_SettingsRightPane",
            nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
            0,
            0,
            100,
            100,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (m_rightScrollPanel)
        {
            SetWindowLongPtrW(m_rightScrollPanel, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }

        m_statusBar = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0,
            0,
            100,
            20,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_tooltip = CreateWindowExW(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASSW,
            nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        if (m_tooltip)
        {
            SetWindowPos(m_tooltip, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, 460);
            SendMessageW(m_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 10000);
            SendMessageW(m_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 250);
        }

        SendMessageW(m_pageView, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_navList->GetHwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_menuFileButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_menuEditButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_menuViewButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_menuPluginsButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_contentSearchEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_chipAllButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_chipToggleButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_chipChoiceButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_chipTextButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_modeBasicButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_modeAdvancedButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_marketplaceDiscoverTabButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_marketplaceInstalledTabButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        if (m_pluginTreeView)
        {
            SendMessageW(m_pluginTreeView, WM_SETFONT, reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        SendMessageW(m_headerTitle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_headerSubtitle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_headerHelpButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetWindowSubclass(m_navList->GetHwnd(), &SettingsWindow::NavListSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

        RegisterHotKey(hwnd, kHotkeyNextSectionId, MOD_CONTROL | MOD_NOREPEAT, VK_TAB);
        RegisterHotKey(hwnd, kHotkeyPrevSectionId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_TAB);
        RegisterHotKey(hwnd, kHotkeyFocusSearchId, MOD_CONTROL | MOD_NOREPEAT, 'L');
        RegisterHotKey(hwnd, kHotkeyFocusNavId, MOD_ALT | MOD_NOREPEAT, '1');
        RegisterHotKey(hwnd, kHotkeyHelpId, MOD_NOREPEAT, VK_F1);

        RefreshTheme();
        PopulatePluginTabs();
        return 0;
    }
    case WM_SIZE:
    {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int navWidth = m_navAnimating
            ? m_navAnimatedWidth
            : (m_navCollapsed ? kNavCollapsedWidth : kNavExpandedWidth);
        const int margin = 10;
        const int topArea = TopAreaHeight();

        int menuButtonWidth = 50;
        int menuButtonGap = 4;
        if (m_themePlatform)
        {
            ThemeResourceResolver* resolver = m_themePlatform->GetResourceResolver();
            if (resolver)
            {
                const std::wstring menuStyle = resolver->GetSelectedMenuStyle();
                if (menuStyle == L"compact")
                {
                    menuButtonWidth = 46;
                    menuButtonGap = 4;
                }
                else if (menuStyle == L"hierarchical")
                {
                    menuButtonWidth = 56;
                    menuButtonGap = 6;
                }
            }
        }

        const int contentLeft = navWidth + (margin * 2);
        const int menuY = margin;

        if (m_menuFileButton)
        {
            MoveWindow(m_menuFileButton, contentLeft, menuY, menuButtonWidth, kMenuBarHeight, TRUE);
            SendMessageW(m_menuFileButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_menuEditButton)
        {
            MoveWindow(m_menuEditButton, contentLeft + (menuButtonWidth + menuButtonGap), menuY, menuButtonWidth, kMenuBarHeight, TRUE);
            SendMessageW(m_menuEditButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_menuViewButton)
        {
            MoveWindow(m_menuViewButton, contentLeft + ((menuButtonWidth + menuButtonGap) * 2), menuY, menuButtonWidth, kMenuBarHeight, TRUE);
            SendMessageW(m_menuViewButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_menuPluginsButton)
        {
            MoveWindow(m_menuPluginsButton, contentLeft + ((menuButtonWidth + menuButtonGap) * 3), menuY, menuButtonWidth + 10, kMenuBarHeight, TRUE);
            SendMessageW(m_menuPluginsButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_headerTitle)
        {
            MoveWindow(m_headerTitle,
                       contentLeft,
                       margin + kMenuBarHeight + kMenuBarGap,
                       width - navWidth - (margin * 3),
                       kHeaderTitleHeight,
                       TRUE);
            SendMessageW(m_headerTitle, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_sectionFont ? m_sectionFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_headerSubtitle)
        {
            MoveWindow(m_headerSubtitle,
                       contentLeft,
                       margin + kMenuBarHeight + kMenuBarGap + kHeaderTitleHeight + kHeaderGap,
                       width - navWidth - (margin * 3),
                       kHeaderSubtitleHeight,
                       TRUE);
            SendMessageW(m_headerSubtitle, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        const int searchY = margin + kMenuBarHeight + kMenuBarGap + kHeaderTitleHeight + kHeaderGap + kHeaderSubtitleHeight + 6;
        const int chipGap = 6;
        const int chipAllWidth = 52;
        const int chipToggleWidth = 76;
        const int chipChoiceWidth = 76;
        const int chipTextWidth = 58;
        const int modeBasicWidth = 64;
        const int modeAdvancedWidth = 82;
        bool showChipAll = true;
        bool showChipToggle = true;
        bool showChipChoice = true;
        bool showChipText = true;

        bool showMarketplaceSubTabs = IsBuiltinPluginsTabSelected();
        const int subTabGap = 6;
        const int subTabWidth = 90;
        const int availableRowWidth = (std::max)(220, width - contentLeft - margin);

        auto computeControlsWidth = [&]() -> int
        {
            int chipCount = 0;
            int chipWidthTotal = 0;
            if (showChipAll)
            {
                chipWidthTotal += chipAllWidth;
                ++chipCount;
            }
            if (showChipToggle)
            {
                chipWidthTotal += chipToggleWidth;
                ++chipCount;
            }
            if (showChipChoice)
            {
                chipWidthTotal += chipChoiceWidth;
                ++chipCount;
            }
            if (showChipText)
            {
                chipWidthTotal += chipTextWidth;
                ++chipCount;
            }

            if (chipCount > 1)
            {
                chipWidthTotal += (chipCount - 1) * chipGap;
            }

            if (chipWidthTotal > 0)
            {
                chipWidthTotal += 10;
            }
            chipWidthTotal += modeBasicWidth + chipGap + modeAdvancedWidth;

            int tabWidthTotal = 0;
            if (showMarketplaceSubTabs)
            {
                tabWidthTotal = (subTabWidth * 2) + subTabGap;
                if (chipWidthTotal > 0)
                {
                    tabWidthTotal += 10;
                }
            }

            return chipWidthTotal + tabWidthTotal;
        };

        // Keep search/filter row stable at smaller widths by degrading optional controls.
        const int kMinSearchWidth = 140;
        while ((kMinSearchWidth + 10 + computeControlsWidth()) > availableRowWidth)
        {
            if (showMarketplaceSubTabs)
            {
                showMarketplaceSubTabs = false;
                continue;
            }
            if (showChipText)
            {
                showChipText = false;
                continue;
            }
            if (showChipChoice)
            {
                showChipChoice = false;
                continue;
            }
            if (showChipToggle)
            {
                showChipToggle = false;
                continue;
            }
            break;
        }

        const int controlsWidth = computeControlsWidth();
        const int searchWidth = (std::max)(kMinSearchWidth, availableRowWidth - controlsWidth - 10);

        if (m_contentSearchEdit)
        {
            MoveWindow(m_contentSearchEdit, contentLeft, searchY, searchWidth, kSearchRowHeight, TRUE);
            SendMessageW(m_contentSearchEdit, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        int nextControlX = contentLeft + searchWidth + 10;
        if (m_chipAllButton)
        {
            ShowWindow(m_chipAllButton, showChipAll ? SW_SHOW : SW_HIDE);
            if (showChipAll)
            {
                MoveWindow(m_chipAllButton, nextControlX, searchY, chipAllWidth, kSearchRowHeight, TRUE);
                nextControlX += chipAllWidth + chipGap;
            }
        }
        if (m_chipToggleButton)
        {
            ShowWindow(m_chipToggleButton, showChipToggle ? SW_SHOW : SW_HIDE);
            if (showChipToggle)
            {
                MoveWindow(m_chipToggleButton, nextControlX, searchY, chipToggleWidth, kSearchRowHeight, TRUE);
                nextControlX += chipToggleWidth + chipGap;
            }
        }
        if (m_chipChoiceButton)
        {
            ShowWindow(m_chipChoiceButton, showChipChoice ? SW_SHOW : SW_HIDE);
            if (showChipChoice)
            {
                MoveWindow(m_chipChoiceButton, nextControlX, searchY, chipChoiceWidth, kSearchRowHeight, TRUE);
                nextControlX += chipChoiceWidth + chipGap;
            }
        }
        if (m_chipTextButton)
        {
            ShowWindow(m_chipTextButton, showChipText ? SW_SHOW : SW_HIDE);
            if (showChipText)
            {
                MoveWindow(m_chipTextButton, nextControlX, searchY, chipTextWidth, kSearchRowHeight, TRUE);
                nextControlX += chipTextWidth + chipGap;
            }
        }

        if (m_modeBasicButton)
        {
            MoveWindow(m_modeBasicButton, nextControlX, searchY, modeBasicWidth, kSearchRowHeight, TRUE);
            SendMessageW(m_modeBasicButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            nextControlX += modeBasicWidth + chipGap;
        }
        if (m_modeAdvancedButton)
        {
            MoveWindow(m_modeAdvancedButton, nextControlX, searchY, modeAdvancedWidth, kSearchRowHeight, TRUE);
            SendMessageW(m_modeAdvancedButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            nextControlX += modeAdvancedWidth + chipGap;
        }

        const int tabsStartX = nextControlX;
        if (m_marketplaceDiscoverTabButton)
        {
            if (showMarketplaceSubTabs)
            {
                ShowWindow(m_marketplaceDiscoverTabButton, SW_SHOW);
                MoveWindow(m_marketplaceDiscoverTabButton, tabsStartX, searchY, subTabWidth, kSearchRowHeight, TRUE);
                SendMessageW(m_marketplaceDiscoverTabButton, WM_SETFONT,
                    reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            }
            else
            {
                ShowWindow(m_marketplaceDiscoverTabButton, SW_HIDE);
            }
        }

        if (m_marketplaceInstalledTabButton)
        {
            if (showMarketplaceSubTabs)
            {
                ShowWindow(m_marketplaceInstalledTabButton, SW_SHOW);
                MoveWindow(m_marketplaceInstalledTabButton, tabsStartX + subTabWidth + subTabGap, searchY, subTabWidth, kSearchRowHeight, TRUE);
                SendMessageW(m_marketplaceInstalledTabButton, WM_SETFONT,
                    reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            }
            else
            {
                ShowWindow(m_marketplaceInstalledTabButton, SW_HIDE);
            }
        }

        if (m_headerHelpButton)
        {
            MoveWindow(m_headerHelpButton,
                       width - margin - 74,
                       menuY,
                       74,
                       kMenuBarHeight,
                       TRUE);
            SendMessageW(m_headerHelpButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_navToggleButton)
        {
            MoveWindow(m_navToggleButton, margin, margin, 54, kMenuBarHeight, TRUE);
            const bool displayCollapsed = m_navAnimating ? m_pendingNavCollapsed : m_navCollapsed;
            SetWindowTextW(m_navToggleButton, displayCollapsed ? L"Show" : L"Hide");
            SendMessageW(m_navToggleButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_navList)
        {
            MoveWindow(m_navList->GetHwnd(), margin, margin + topArea, navWidth, height - (margin * 3) - topArea - kStatusHeight, TRUE);
        }
        if (m_pageView)
        {
            MoveWindow(m_pageView,
                       navWidth + (margin * 2),
                       margin + topArea,
                       width - navWidth - (margin * 3),
                       height - (margin * 3) - topArea - kStatusHeight,
                       TRUE);
        }
        if (m_rightScrollPanel)
        {
            MoveWindow(m_rightScrollPanel,
                       navWidth + (margin * 2),
                       margin + topArea,
                       width - navWidth - (margin * 3),
                       height - (margin * 3) - topArea - kStatusHeight,
                       TRUE);
        }
        if (m_pluginTreeView)
        {
            MoveWindow(m_pluginTreeView,
                       navWidth + (margin * 2),
                       margin + topArea,
                       width - navWidth - (margin * 3),
                       height - (margin * 3) - topArea - kStatusHeight,
                       TRUE);
            SendMessageW(m_pluginTreeView, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_statusBar)
        {
            MoveWindow(m_statusBar,
                       navWidth + (margin * 2),
                       height - margin - kStatusHeight,
                       width - navWidth - (margin * 3),
                       kStatusHeight,
                       TRUE);
            SendMessageW(m_statusBar, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_rightScrollPanel && IsWindowVisible(m_rightScrollPanel) && !m_fieldControlLayouts.empty())
        {
            RelayoutScrollPanelChildren();
        }
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info)
        {
            info->ptMinTrackSize.x = 760;
            info->ptMinTrackSize.y = 540;
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        if (m_rightScrollPanel && IsWindowVisible(m_rightScrollPanel))
        {
            POINT cursor{};
            GetCursorPos(&cursor);
            RECT panelRect{};
            GetWindowRect(m_rightScrollPanel, &panelRect);
            if (PtInRect(&panelRect, cursor))
            {
                SendMessageW(m_rightScrollPanel, msg, wParam, lParam);
                return 0;
            }
        }
        break;
    }
    case WM_COMMAND:
    {
        const int id   = LOWORD(wParam);
        const int code = HIWORD(wParam);
        HWND hwndCtrl  = reinterpret_cast<HWND>(lParam);

        if (id == kNavToggleId && code == BN_CLICKED)
        {
            const DWORD nowTick = GetTickCount();
            if (m_lastNavToggleTick != 0 && (nowTick - m_lastNavToggleTick) < kNavToggleDebounceMs)
            {
                return 0;
            }
            m_lastNavToggleTick = nowTick;

            const bool desiredCollapsed = !m_navCollapsed;
            if (m_settingsRegistry)
            {
                m_settingsRegistry->SetValue(L"settings.ui.nav_collapsed", desiredCollapsed ? L"true" : L"false");
            }
            m_pendingNavCollapsed = desiredCollapsed;
            PostMessageW(hwnd, kMsgApplyNavCollapsed, desiredCollapsed ? 1 : 0, 0);
            return 0;
        }

        if (id == kNavId && code == LBN_SELCHANGE)
        {
            ShowSelectedPluginTab();
            return 0;
        }

        if ((id == kMenuFileId || id == kMenuEditId || id == kMenuViewId || id == kMenuPluginsId) && code == BN_CLICKED)
        {
            ShowMenuBarPopup(id);
            return 0;
        }

        if (id == kHeaderHelpId && code == BN_CLICKED)
        {
            ShowMenuBarPopup(id);
            return 0;
        }

        if (id == kContentSearchId && code == EN_CHANGE)
        {
            wchar_t searchBuf[256]{};
            GetWindowTextW(m_contentSearchEdit, searchBuf, static_cast<int>(std::size(searchBuf)));
            m_contentSearchQuery = searchBuf;
            ShowSelectedPluginTab();
            return 0;
        }

        if (IsContentChipId(id) && code == BN_CLICKED)
        {
            int newChip = 0;
            if (id == kChipToggleId)
            {
                newChip = 1;
            }
            else if (id == kChipChoiceId)
            {
                newChip = 2;
            }
            else if (id == kChipTextId)
            {
                newChip = 3;
            }

            m_activeContentChip = newChip;
            if (m_chipAllButton) InvalidateRect(m_chipAllButton, nullptr, FALSE);
            if (m_chipToggleButton) InvalidateRect(m_chipToggleButton, nullptr, FALSE);
            if (m_chipChoiceButton) InvalidateRect(m_chipChoiceButton, nullptr, FALSE);
            if (m_chipTextButton) InvalidateRect(m_chipTextButton, nullptr, FALSE);
            ShowSelectedPluginTab();
            return 0;
        }

        if ((id == kModeBasicId || id == kModeAdvancedId) && code == BN_CLICKED)
        {
            const bool newAdvanced = (id == kModeAdvancedId);
            if (m_showAdvancedSettings != newAdvanced)
            {
                m_showAdvancedSettings = newAdvanced;
                if (m_settingsRegistry)
                {
                    m_settingsRegistry->SetValue(L"settings.ui.advanced_mode", m_showAdvancedSettings ? L"true" : L"false");
                }
            }

            if (m_modeBasicButton) InvalidateRect(m_modeBasicButton, nullptr, FALSE);
            if (m_modeAdvancedButton) InvalidateRect(m_modeAdvancedButton, nullptr, FALSE);
            ShowSelectedPluginTab();
            return 0;
        }

        if ((id == kMarketplaceDiscoverTabId || id == kMarketplaceInstalledTabId) && code == BN_CLICKED)
        {
            m_marketplaceSubTab = (id == kMarketplaceInstalledTabId) ? 1 : 0;
            if (m_marketplaceDiscoverTabButton)
            {
                InvalidateRect(m_marketplaceDiscoverTabButton, nullptr, FALSE);
            }
            if (m_marketplaceInstalledTabButton)
            {
                InvalidateRect(m_marketplaceInstalledTabButton, nullptr, FALSE);
            }
            ShowSelectedPluginTab();
            return 0;
        }

        // Handle interactive field controls
        if (m_controlFieldMap.count(id))
        {
            HandleFieldControlChange(id, code, hwndCtrl);
            return 0;
        }
        break;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        RefreshTheme();
        return 0;
    case WM_HOTKEY:
    {
        switch (static_cast<int>(wParam))
        {
        case kHotkeyNextSectionId:
        case kHotkeyPrevSectionId:
            if (m_navList && !m_pluginTabs.empty())
            {
                const int direction = (wParam == kHotkeyPrevSectionId) ? -1 : 1;
                int current = m_navList ? static_cast<int>(m_navList->GetSelectedIndex()) : 0;
                int count = static_cast<int>(m_pluginTabs.size());
                int next = (current + direction + count) % count;
                if (m_navList) m_navList->SetSelectedIndex(next);
                ShowSelectedPluginTab();
            }
            return 0;
        case kHotkeyFocusSearchId:
            FocusPreferredSearchField();
            return 0;
        case kHotkeyFocusNavId:
            if (m_navList) {
                SetFocus(m_navList->GetHwnd());
            }
            return 0;
        case kHotkeyHelpId:
            HandleMenuBarCommand(kMenuCmdKeyboardShortcuts);
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_TIMER:
        if (wParam == kNavAnimationTimerId)
        {
            StepNavCollapseAnimation();
            return 0;
        }
        if (wParam == kMarketplaceSpinnerTimerId)
        {
            if (m_marketplaceSpinnerActive)
            {
                m_marketplaceSpinnerFrame = (m_marketplaceSpinnerFrame + 1) % 4;
                if (m_navList) {
                    size_t selection = m_navList->GetSelectedIndex();
                    UpdateShellHeaderAndStatus(selection);
                }
            }
            return 0;
        }
        break;
    case kMsgApplyNavCollapsed:
    {
        const bool requestedCollapsed = (wParam != 0);
        StartNavCollapseAnimation(requestedCollapsed);
        return 0;
    }
    case WM_MEASUREITEM:
    {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure && measure->CtlID == kNavId)
        {
            measure->itemHeight = m_navCollapsed ? 44 : 40;
            const int navWidth = m_navAnimating
                ? m_navAnimatedWidth
                : (m_navCollapsed ? kNavCollapsedWidth : kNavExpandedWidth);
            measure->itemWidth = static_cast<UINT>(navWidth);
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM:
    {
        auto* drawInfo = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (drawInfo && drawInfo->CtlID == kNavId)
        {
            DrawNavItem(drawInfo);
            return TRUE;
        }
        if (drawInfo && drawInfo->CtlType == ODT_BUTTON)
        {
            if (drawInfo->CtlID == kNavToggleId ||
                drawInfo->CtlID == kHeaderHelpId ||
                drawInfo->CtlID == kMenuFileId ||
                drawInfo->CtlID == kMenuEditId ||
                drawInfo->CtlID == kMenuViewId ||
                drawInfo->CtlID == kMenuPluginsId ||
                drawInfo->CtlID == kModeBasicId ||
                drawInfo->CtlID == kModeAdvancedId ||
                drawInfo->CtlID == kMarketplaceDiscoverTabId ||
                drawInfo->CtlID == kMarketplaceInstalledTabId ||
                IsContentChipId(static_cast<int>(drawInfo->CtlID)))
            {
                DrawShellButton(drawInfo);
                return TRUE;
            }

            const auto it = m_controlFieldMap.find(static_cast<int>(drawInfo->CtlID));
            if (it != m_controlFieldMap.end() && it->second.type == SettingsFieldType::Bool)
            {
                DrawToggleControl(drawInfo);
                return TRUE;
            }
        }
        break;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);
        const bool isNav = (m_navList && ctrl == m_navList->GetHwnd());

        if (ctrl == m_headerSubtitle)
        {
            // In cyber mode dim-cyan subtitle — creates tech breadcrumb feel
            const COLORREF subtitleColor = IsCyberTheme(m_windowColor)
                ? BlendColor(m_accentColor, m_windowColor, 140)
                : m_subtleTextColor;
            SetTextColor(hdc, subtitleColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
        }

        if (ctrl == m_headerTitle || ctrl == m_statusBar)
        {
            // In cyber mode the title glows with the accent color
            const COLORREF titleColor = IsCyberTheme(m_windowColor) ? m_accentColor : m_textColor;
            SetTextColor(hdc, titleColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
        }

        if (ctrl == m_headerHelpButton ||
            ctrl == m_menuFileButton ||
            ctrl == m_menuEditButton ||
            ctrl == m_menuViewButton ||
            ctrl == m_menuPluginsButton)
        {
            SetTextColor(hdc, m_textColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
        }

        if (ctrl == m_contentSearchEdit)
        {
            SetTextColor(hdc, m_textColor);
            SetBkColor(hdc, m_surfaceColor);
            return reinterpret_cast<LRESULT>(m_surfaceBrush ? m_surfaceBrush : GetStockObject(WHITE_BRUSH));
        }

        if (m_rightPaneTextStatics.find(ctrl) != m_rightPaneTextStatics.end())
        {
            // Use an opaque fill matching the surface.  NULL_BRUSH (transparent)
            // leaves background content showing through during repaints, which
            // is the root cause of section headers ghosting over other areas.
            SetTextColor(hdc, m_textColor);
            SetBkColor(hdc, m_surfaceColor);
            return reinterpret_cast<LRESULT>(m_surfaceBrush ? m_surfaceBrush : GetStockObject(WHITE_BRUSH));
        }

        SetTextColor(hdc, m_textColor);
        SetBkColor(hdc, isNav ? m_navColor : m_surfaceColor);

        if (isNav)
        {
            return reinterpret_cast<LRESULT>(m_navBrush ? m_navBrush : m_surfaceBrush);
        }
        return reinterpret_cast<LRESULT>(m_surfaceBrush ? m_surfaceBrush : GetStockObject(DC_BRUSH));
    }
    case WM_ERASEBKGND:
    {
        if (!m_windowBrush)
        {
            break;
        }

        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, m_windowBrush);

        // Premium shell framing inspired by VS Code layout rhythm.
        const int navWidth = m_navAnimating
            ? m_navAnimatedWidth
            : (m_navCollapsed ? kNavCollapsedWidth : kNavExpandedWidth);
        const int margin = 10;
        const int topArea = TopAreaHeight();
        const COLORREF divider = BlendColor(m_windowColor, m_textColor, 28);

        const RECT navPanel{
            margin,
            margin + topArea,
            margin + navWidth,
            rect.bottom - margin - kStatusHeight
        };
        const RECT contentPanel{
            navWidth + (margin * 2),
            margin + topArea,
            rect.right - margin,
            rect.bottom - margin - kStatusHeight
        };

        const bool cyber = IsCyberTheme(m_windowColor);

        if (cyber)
        {
            // Cyberpunk: deep near-black panels with sharp geometry — no rounded corners
            const COLORREF navPanelFill   = m_navColor;
            const COLORREF contentFill    = m_surfaceColor;
            const COLORREF dimCyanBorder  = BlendColor(m_navColor, m_accentColor, 50);
            const COLORREF contentBorderC = BlendColor(m_surfaceColor, m_accentColor, 36);

            HBRUSH navBrush   = CreateSolidBrush(navPanelFill);
            HBRUSH contBrush  = CreateSolidBrush(contentFill);
            HPEN   navPen     = CreatePen(PS_SOLID, 1, dimCyanBorder);
            HPEN   contPen    = CreatePen(PS_SOLID, 1, contentBorderC);

            HGDIOBJ oldObj = SelectObject(hdc, navBrush);
            SelectObject(hdc, navPen);
            Rectangle(hdc, navPanel.left, navPanel.top, navPanel.right, navPanel.bottom);

            SelectObject(hdc, contBrush);
            SelectObject(hdc, contPen);
            Rectangle(hdc, contentPanel.left, contentPanel.top, contentPanel.right, contentPanel.bottom);

            SelectObject(hdc, oldObj);
            DeleteObject(navBrush);
            DeleteObject(contBrush);
            DeleteObject(navPen);
            DeleteObject(contPen);

            // Full-width cyan scan-line at top of content panel — the signature cyber detail
            HPEN scanPen = CreatePen(PS_SOLID, 1, m_accentColor);
            HPEN oldScan = reinterpret_cast<HPEN>(SelectObject(hdc, scanPen));
            MoveToEx(hdc, contentPanel.left, contentPanel.top, nullptr);
            LineTo(hdc, contentPanel.right, contentPanel.top);
            SelectObject(hdc, oldScan);
            DeleteObject(scanPen);

            // Bloom line — diffused glow 1px below the scan-line
            HPEN bloomPen = CreatePen(PS_SOLID, 1, BlendColor(m_accentColor, m_windowColor, 60));
            HPEN oldBloom = reinterpret_cast<HPEN>(SelectObject(hdc, bloomPen));
            MoveToEx(hdc, contentPanel.left, contentPanel.top + 1, nullptr);
            LineTo(hdc, contentPanel.right, contentPanel.top + 1);
            SelectObject(hdc, oldBloom);
            DeleteObject(bloomPen);
        }
        else
        {
            const COLORREF navPanelFill   = BlendColor(m_navColor, m_windowColor, 46);
            const COLORREF contentPanelFill = BlendColor(m_surfaceColor, m_windowColor, 24);
            const COLORREF panelBorder    = BlendColor(m_windowColor, m_textColor, 36);
            const COLORREF accentEdge     = BlendColor(m_accentColor, m_surfaceColor, 48);

            HBRUSH navBrush     = CreateSolidBrush(navPanelFill);
            HBRUSH contentBrush = CreateSolidBrush(contentPanelFill);
            HPEN   panelPen     = CreatePen(PS_SOLID, 1, panelBorder);

            HGDIOBJ oldPenObj   = SelectObject(hdc, panelPen);
            HGDIOBJ oldBrushObj = SelectObject(hdc, navBrush);
            RoundRect(hdc, navPanel.left, navPanel.top, navPanel.right, navPanel.bottom, 10, 10);

            SelectObject(hdc, contentBrush);
            RoundRect(hdc, contentPanel.left, contentPanel.top, contentPanel.right, contentPanel.bottom, 10, 10);

            SelectObject(hdc, oldBrushObj);
            SelectObject(hdc, oldPenObj);
            DeleteObject(navBrush);
            DeleteObject(contentBrush);
            DeleteObject(panelPen);

            HPEN accentPen = CreatePen(PS_SOLID, 2, accentEdge);
            HPEN oldAccentPen = reinterpret_cast<HPEN>(SelectObject(hdc, accentPen));
            MoveToEx(hdc, contentPanel.left + 8, contentPanel.top + 1, nullptr);
            LineTo(hdc, contentPanel.right - 8, contentPanel.top + 1);
            SelectObject(hdc, oldAccentPen);
            DeleteObject(accentPen);
        }

        HPEN pen = CreatePen(PS_SOLID, 1, divider);
        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));

        const int verticalX = navWidth + (margin * 2) - 8;
        MoveToEx(hdc, verticalX, margin, nullptr);
        LineTo(hdc, verticalX, rect.bottom - margin);

        const int horizontalY = margin + topArea - 8;
        MoveToEx(hdc, navWidth + (margin * 2), horizontalY, nullptr);
        LineTo(hdc, rect.right - margin, horizontalY);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        return 1;
    }
    case WM_CLOSE:
    {
        CommitPendingTextFieldEdits();
        const bool closeToTray = m_settingsRegistry
            ? (m_settingsRegistry->GetValue(L"spaces.window.close_to_tray", L"true") == L"true")
            : true;
        if (closeToTray)
        {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        UnregisterHotKey(hwnd, kHotkeyNextSectionId);
        UnregisterHotKey(hwnd, kHotkeyPrevSectionId);
        UnregisterHotKey(hwnd, kHotkeyFocusSearchId);
        UnregisterHotKey(hwnd, kHotkeyFocusNavId);
        UnregisterHotKey(hwnd, kHotkeyHelpId);
        KillTimer(hwnd, kNavAnimationTimerId);
        KillTimer(hwnd, kMarketplaceSpinnerTimerId);
        DestroyThemeBrushes();
        DestroyFonts();
        if (m_tooltip)
        {
            DestroyWindow(m_tooltip);
            m_tooltip = nullptr;
        }
        m_hwnd = nullptr;
        m_navToggleButton = nullptr;
        m_menuFileButton = nullptr;
        m_menuEditButton = nullptr;
        m_menuViewButton = nullptr;
        m_menuPluginsButton = nullptr;
        m_contentSearchEdit = nullptr;
        m_chipAllButton = nullptr;
        m_chipToggleButton = nullptr;
        m_chipChoiceButton = nullptr;
        m_chipTextButton = nullptr;
        m_marketplaceDiscoverTabButton = nullptr;
        m_marketplaceInstalledTabButton = nullptr;
        ClearPluginTree();
        m_pluginTreeView = nullptr;
        m_headerTitle = nullptr;
        m_headerSubtitle = nullptr;
        m_headerHelpButton = nullptr;
        delete m_navList;
        m_navList = nullptr;
        m_pageView = nullptr;
        m_rightScrollPanel = nullptr;
        m_statusBar = nullptr;
        m_plugins.clear();
        m_pluginTabs.clear();
        // Field control HWNDs are destroyed automatically as children of this window.
        // Just clear the tracking data.
        m_fieldControls.clear();
        m_fieldControlLayouts.clear();
        m_controlFieldMap.clear();
        m_sectionCardRects.clear();
        m_rightPaneTextStatics.clear();
        m_rightPaneScrollY = 0;
        m_rightPaneContentHeight = 0;
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

