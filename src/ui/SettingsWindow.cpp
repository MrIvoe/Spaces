#include "ui/SettingsWindow.h"

#include "core/PluginHubSync.h"
#include "core/ThemePlatform.h"
#include "extensions/PluginSettingsRegistry.h"
#include "Win32Helpers.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

namespace
{
    constexpr int kHeaderTitleHeight = 30;
    constexpr int kHeaderSubtitleHeight = 22;
    constexpr int kHeaderGap = 4;
    constexpr int kStatusHeight = 28;
    constexpr int kRightPaneTransitionStartOffset = 18;
    constexpr int kRightPaneTransitionStep = 6;

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
}

bool SettingsWindow::ShowScaffold(const std::vector<SettingsPageView>& pages,
                                   const std::vector<PluginStatusView>& plugins,
                                   PluginSettingsRegistry* registry,
                                   const ThemePlatform* themePlatform)
{
    m_settingsRegistry = registry;
    m_themePlatform = themePlatform;
    m_navCollapsed = (m_settingsRegistry && m_settingsRegistry->GetValue(L"settings.ui.nav_collapsed", L"false") == L"true");
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

    const std::wstring themeText = (m_themeMode == ThemeMode::Dark) ? L"dark" : L"light";
    Win32Helpers::LogInfo(L"Settings window opened (theme='" + themeText + L"').");

    return true;
}

bool SettingsWindow::EnsureWindow()
{
    if (m_hwnd)
    {
        return true;
    }

    static const wchar_t* kClassName = L"SimpleFences_SettingsWindow";
    static const wchar_t* kScrollPanelClass = L"SimpleFences_SettingsRightPane";

    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWindow::WndProcStatic;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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

    m_hwnd = CreateWindowExW(
        0,
        kClassName,
        L"SimpleFences Settings",
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
        Win32Helpers::LogError(L"Settings window CreateWindowEx failed");
        return false;
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

    const int basePx = (20 * textScale) / 100;
    const int sectionPx = (24 * textScale) / 100;
    const int navPx = (22 * textScale) / 100;

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

    if (m_pageView)
    {
        SendMessageW(m_pageView, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    if (m_navList)
    {
        SendMessageW(m_navList, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
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
    if (m_statusBar)
    {
        SendMessageW(m_statusBar, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }

    if (m_hwnd)
    {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
    if (m_navList)
    {
        InvalidateRect(m_navList, nullptr, TRUE);
    }
    if (m_pageView)
    {
        InvalidateRect(m_pageView, nullptr, TRUE);
    }
    if (m_rightScrollPanel)
    {
        InvalidateRect(m_rightScrollPanel, nullptr, TRUE);
    }
}

std::wstring SettingsWindow::ResolvePluginDisplayName(const std::wstring& pluginId, const std::vector<PluginStatusView>& plugins) const
{
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

std::wstring SettingsWindow::ResolvePluginIcon(const PluginStatusView& plugin) const
{
    for (const auto& capability : plugin.capabilities)
    {
        if (capability == L"settings_pages")
        {
            return L"\uE713";
        }
        if (capability == L"appearance")
        {
            return L"\uE790";
        }
        if (capability == L"widgets")
        {
            return L"\uE9CA";
        }
        if (capability == L"fence_content_provider")
        {
            return L"\uE8B7";
        }
        if (capability == L"tray_contributions")
        {
            return L"\uEA8F";
        }
    }

    return L"\uE943";
}

void SettingsWindow::BuildPluginTabs(const std::vector<PluginStatusView>& plugins)
{
    m_pluginTabs.clear();

    PluginTab overview;
    overview.pluginId = L"__overview__";
    overview.iconGlyph = L"\uE80F";
    overview.title = L"Overview";
    m_pluginTabs.push_back(std::move(overview));

    std::unordered_map<std::wstring, size_t> byPlugin;
    byPlugin.emplace(L"__overview__", 0);

    for (const auto& plugin : plugins)
    {
        PluginTab tab;
        tab.pluginId = plugin.id;
        tab.iconGlyph = ResolvePluginIcon(plugin);
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
            tab.iconGlyph = L"\uE943";
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
        fallback.iconGlyph = L"\uE80F";
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
    {
        return;
    }

    const LRESULT previousSelection = SendMessageW(m_navList, LB_GETCURSEL, 0, 0);

    SendMessageW(m_navList, LB_RESETCONTENT, 0, 0);

    for (const auto& tab : m_pluginTabs)
    {
        std::wstring label = m_navCollapsed ? tab.iconGlyph : (tab.iconGlyph + L"  " + tab.title);
        SendMessageW(m_navList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }

    m_navHoverIndex = -1;

    int selectionToApply = 0;
    if (previousSelection >= 0)
    {
        selectionToApply = static_cast<int>(previousSelection);
    }
    if (selectionToApply >= static_cast<int>(m_pluginTabs.size()))
    {
        selectionToApply = static_cast<int>(m_pluginTabs.empty() ? 0 : (m_pluginTabs.size() - 1));
    }
    SendMessageW(m_navList, LB_SETCURSEL, static_cast<WPARAM>(selectionToApply), 0);

    InvalidateRect(m_navList, nullptr, TRUE);
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
    const LRESULT selection = SendMessageW(m_navList, LB_GETCURSEL, 0, 0);
    if (selection == -1)
    {
        return;
    }

    const size_t tabIndex = static_cast<size_t>(selection);
    if (tabIndex >= m_pluginTabs.size())
    {
        return;
    }

    if (!m_pageView)
    {
        return;
    }

    UpdateShellHeaderAndStatus(tabIndex);

    // Check whether any page in this tab declares interactive fields.
    const auto& tab = m_pluginTabs[tabIndex];
    bool hasFields = false;
    if (tab.pluginId != L"__overview__")
    {
        for (const size_t pi : tab.pageIndexes)
        {
            if (pi < m_pages.size() && !m_pages[pi].fields.empty())
            {
                hasFields = true;
                break;
            }
        }
    }

    // Suppress redraws on both the main window and the scroll panel while
    // we tear down old controls and create new ones.  This prevents the
    // half-constructed control tree from being painted, which is what
    // produces the "text glitching over everything" visual artifact.
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    if (m_rightScrollPanel)
    {
        SendMessageW(m_rightScrollPanel, WM_SETREDRAW, FALSE, 0);
    }
    ClearFieldControls();

    if (hasFields)
    {
        // Hide the read-only EDIT; render interactive controls instead.
        ShowWindow(m_pageView, SW_HIDE);
        if (m_rightScrollPanel)
        {
            // Show AFTER controls are populated so nothing flashes.
            ShowWindow(m_rightScrollPanel, SW_HIDE);
        }
        SetWindowTextW(m_pageView, L"");

        RECT clientRect{};
        GetClientRect(m_hwnd, &clientRect);
        const int width = clientRect.right - clientRect.left;
        const int navWidth = m_navCollapsed ? 64 : 280;
        const int margin   = 10;
        const int topArea = kHeaderTitleHeight + kHeaderSubtitleHeight + kHeaderGap + 16;
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
        if (m_rightScrollPanel)
        {
            SendMessageW(m_rightScrollPanel, WM_SETREDRAW, TRUE, 0);
            ShowWindow(m_rightScrollPanel, SW_HIDE);
        }
        ShowWindow(m_pageView, SW_SHOW);
        SetWindowTextW(m_pageView, BuildSelectedTabContent(tabIndex).c_str());
    }

    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);

    RECT clientRect{};
    GetClientRect(m_hwnd, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    const int navWidth = m_navCollapsed ? 64 : 280;
    const int margin = 10;
    const int topArea = kHeaderTitleHeight + kHeaderSubtitleHeight + kHeaderGap + 16;

    RECT rightPaneRect{};
    rightPaneRect.left = navWidth + (margin * 2);
    rightPaneRect.top = margin + topArea;
    rightPaneRect.right = width - margin;
    rightPaneRect.bottom = height - margin - kStatusHeight;

    if (rightPaneRect.right > rightPaneRect.left && rightPaneRect.bottom > rightPaneRect.top)
    {
        if (hasFields && m_rightScrollPanel)
        {
            InvalidateRect(m_rightScrollPanel, nullptr, TRUE);
            UpdateWindow(m_rightScrollPanel);
        }
        else if (m_pageView)
        {
            InvalidateRect(m_pageView, nullptr, TRUE);
            UpdateWindow(m_pageView);
        }
    }

    // Skip the slide-in animation for interactive field pages hosted in the
    // scroll panel child window — SetWindowPos animation on those child
    // HWNDs causes ghost trails against uncleared background regions.
    if (!hasFields)
    {
        BeginRightPaneTransition();
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
    text += L"Phase 0.0.013 settings shell is active.\r\n";
    text += L"Core fences remain first-class and stable while plugins extend behavior.\r\n\r\n";
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
    text += L"Plugins\r\n\r\n";
    text += L"Use the Plugin Hub controls above to sync plugins into %LOCALAPPDATA%\\SimpleFences\\plugins.\r\n\r\n";

    if (plugins.empty())
    {
        text += L"No plugins registered.\r\n";
        return text;
    }

    for (const auto& plugin : plugins)
    {
        text += plugin.displayName + L" (" + plugin.id + L", v" + plugin.version + L")\r\n";
        text += L"State: " + PluginStateText(plugin) + L"\r\n";
        text += L"Capabilities: " + JoinCapabilities(plugin.capabilities) + L"\r\n";
        if (!plugin.lastError.empty())
        {
            text += L"Error: " + plugin.lastError + L"\r\n";
        }
        text += L"\r\n";
    }

    return text;
}

std::wstring SettingsWindow::BuildDiagnosticsContent(const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;
    text += L"Diagnostics\r\n\r\n";

    int failed = 0;
    int disabled = 0;
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
    }

    text += L"Plugin failures: " + std::to_wstring(failed) + L"\r\n";
    text += L"Plugins disabled: " + std::to_wstring(disabled) + L"\r\n\r\n";
    text += L"This shell can be expanded into a richer page host with controls per plugin page.\r\n";
    return text;
}

std::wstring SettingsWindow::BuildGenericPageContent(const SettingsPageView& page) const
{
    if (page.pluginId == L"builtin.core_commands")
    {
        if (page.pageId == L"core.behavior")
        {
            return L"Behavior\r\n\r\n"
                   L"- New fence command is routed through kernel command id 'fence.create'.\r\n"
                   L"- Fence creation remains core-owned for stability.\r\n"
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
               L"- Current entries: New Fence, Settings, Exit.\r\n"
               L"- Suggested future toggles: close-to-tray behavior, startup visibility, menu ordering profile.";
    }

    if (page.pluginId == L"builtin.appearance" && page.pageId == L"appearance.theme")
    {
        const std::wstring themeText = (m_themeMode == ThemeMode::Dark) ? L"dark" : L"light";
        std::wstring styleText = L"system";
        if (m_themeStyle == ThemeStyle::Discord)
        {
            styleText = L"discord";
        }
        else if (m_themeStyle == ThemeStyle::Fences)
        {
            styleText = L"fences";
        }
        else if (m_themeStyle == ThemeStyle::GitHubDark)
        {
            styleText = L"github_dark";
        }
        else if (m_themeStyle == ThemeStyle::GitHubDarkDimmed)
        {
            styleText = L"github_dark_dimmed";
        }
        else if (m_themeStyle == ThemeStyle::GitHubLight)
        {
            styleText = L"github_light";
        }
        else if (m_themeStyle == ThemeStyle::Custom)
        {
            styleText = L"custom";
        }
        return L"Theme\r\n\r\n"
               L"- Theme engine supports mode + style profiles for future app-wide theming.\r\n"
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
               L"- Suggested future toggles: show fence quick actions, advanced context entries, safety confirmations.";
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
    if (m_hwnd)
    {
        KillTimer(m_hwnd, kRightPaneTransitionTimerId);
    }
    m_rightPaneTransitionOffset = 0;

    for (HWND hwnd : m_fieldControls)
    {
        if (hwnd)
        {
            DestroyWindow(hwnd);
        }
    }
    m_fieldControls.clear();
    m_controlFieldMap.clear();
    m_sectionCardRects.clear();
    m_rightPaneTargetRects.clear();
    m_rightPaneTextStatics.clear();
    // Reset ID counter so IDs don't climb unboundedly across tab switches.
    m_nextControlId = 2000;
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
        subtitle = L"System status, plugin health, and global settings summary.";
    }
    else
    {
        subtitle = L"Configure plugin behavior and appearance settings.";
        if (!tab.pageIndexes.empty())
        {
            subtitle += L" Pages: " + std::to_wstring(tab.pageIndexes.size()) + L".";
        }
    }

    if (m_headerTitle)
    {
        SetWindowTextW(m_headerTitle, headerText.c_str());
    }
    if (m_headerSubtitle)
    {
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
        status << L"Loaded plugins: " << loadedCount << L" / " << m_plugins.size()
               << L"   |   Failed: " << failedCount
               << L"   |   Active tab: " << tab.title
               << L"   |   Ctrl+Tab switch  F1 help";
        SetWindowTextW(m_statusBar, status.str().c_str());
    }
}

void SettingsWindow::BeginRightPaneTransition()
{
    if (!m_hwnd)
    {
        return;
    }

    KillTimer(m_hwnd, kRightPaneTransitionTimerId);
    m_rightPaneTransitionOffset = kRightPaneTransitionStartOffset;
    ApplyRightPaneTransitionLayout();
    SetTimer(m_hwnd, kRightPaneTransitionTimerId, 15, nullptr);
}

void SettingsWindow::ApplyRightPaneTransitionLayout()
{
    int scrollY = 0;
    if (m_rightScrollPanel)
    {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        if (GetScrollInfo(m_rightScrollPanel, SB_VERT, &si))
        {
            scrollY = si.nPos;
        }
    }

    for (const auto& entry : m_rightPaneTargetRects)
    {
        HWND hwnd = entry.first;
        const RECT& rc = entry.second;
        if (!hwnd || !IsWindow(hwnd))
        {
            continue;
        }

        SetWindowPos(hwnd,
                     nullptr,
                     rc.left + m_rightPaneTransitionOffset,
                     rc.top - scrollY,
                     rc.right - rc.left,
                     rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
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

    HBRUSH background = CreateSolidBrush(m_navColor);
    FillRect(hdc, &rc, background);
    DeleteObject(background);

    RECT pillRc = rc;
    InflateRect(&pillRc, -6, -4);

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
        accentRc.right = accentRc.left + 4;
        HBRUSH accentBrush = CreateSolidBrush(m_accentColor);
        FillRect(hdc, &accentRc, accentBrush);
        DeleteObject(accentBrush);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_textColor);
    SelectObject(hdc, m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT));

    if (m_navCollapsed)
    {
        DrawTextW(hdc, tab.iconGlyph.c_str(), -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        RECT glyphRc = pillRc;
        glyphRc.left += 12;
        glyphRc.right = glyphRc.left + 24;
        DrawTextW(hdc, tab.iconGlyph.c_str(), -1, &glyphRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT textRc = pillRc;
        textRc.left = glyphRc.right + 12;
        DrawTextW(hdc, tab.title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if ((drawInfo->itemState & ODS_FOCUS) != 0)
    {
        RECT focusRc = pillRc;
        InflateRect(&focusRc, -2, -2);
        DrawFocusRect(hdc, &focusRc);
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
        if (hoverIndex != self->m_navHoverIndex)
        {
            self->m_navHoverIndex = hoverIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE:
        if (self->m_navHoverIndex != -1)
        {
            self->m_navHoverIndex = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
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
            return self->DrawScrollPanelBkgnd(reinterpret_cast<HDC>(wParam));
        }
        break;
    case WM_MOUSEWHEEL:
        if (self)
        {
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
            case SB_THUMBTRACK:
                newPos = HIWORD(wParam);
                break;
            default:
                break;
            }

            const int maxPos = (std::max)(0, si.nMax - static_cast<int>(si.nPage) + 1);
            newPos = (std::max)(0, (std::min)(newPos, maxPos));
            if (newPos != si.nPos)
            {
                si.fMask = SIF_POS;
                si.nPos = newPos;
                SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
                self->ApplyRightPaneTransitionLayout();
                InvalidateRect(hwnd, nullptr, TRUE);
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
            SetTextColor(hdc, self->m_textColor);
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
    if (newPos == si.nPos)
    {
        return;
    }

    si.fMask = SIF_POS;
    si.nPos = newPos;
    SetScrollInfo(m_rightScrollPanel, SB_VERT, &si, TRUE);
    ApplyRightPaneTransitionLayout();
    InvalidateRect(m_rightScrollPanel, nullptr, TRUE);
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

    const COLORREF cardFill = BlendColor(m_surfaceColor, m_windowColor, 84);
    const COLORREF cardBorder = BlendColor(m_accentColor, m_windowColor, 48);
    HBRUSH cardBrush = CreateSolidBrush(cardFill);
    HPEN cardPen = CreatePen(PS_SOLID, 1, cardBorder);
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, cardPen));
    HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, cardBrush));

    for (const RECT& cardRect : m_sectionCardRects)
    {
        Rectangle(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(cardBrush);
    DeleteObject(cardPen);
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

    const int leftPad = 12;
    const int topPad = 12;
    const int rightPad = 12;
    const int rightPaneW = (std::max)(120, panelWidth - leftPad - rightPad);

    const HFONT hFont = m_baseFont ? m_baseFont : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HFONT hHeaderFont = m_sectionFont ? m_sectionFont : hFont;
    const int   rowH        = 34;
    const int   rowGap      = 6;
    const int   sectionGap  = 22;
    const int   labelWidth  = 280;
    const int   ctrlWidth   = 290;
    const int   ctrlGap     = 14;

    int y = topPad;

    const auto& tab = m_pluginTabs[tabIndex];

    for (const size_t pi : tab.pageIndexes)
    {
        if (pi >= m_pages.size())
        {
            continue;
        }
        const auto& page = m_pages[pi];
        if (page.fields.empty())
        {
            continue;
        }

        const int sectionStartY = y;
        const int sectionHeight = rowH + 4 + (static_cast<int>(page.fields.size()) * (rowH + rowGap)) + sectionGap;
        RECT cardRect{};
        cardRect.left = leftPad - 14;
        cardRect.top = sectionStartY - 10;
        cardRect.right = leftPad + rightPaneW;
        cardRect.bottom = sectionStartY + sectionHeight - 6;
        m_sectionCardRects.push_back(cardRect);

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
            m_rightPaneTargetRects.emplace(hHeader, RECT{leftPad, y, leftPad + rightPaneW, y + rowH});
            m_rightPaneTextStatics.insert(hHeader);
        }
        y += rowH + 4;

        // --- Fields -------------------------------------------------------------
        // Sort by order for display
        std::vector<const SettingsFieldDescriptor*> sorted;
        sorted.reserve(page.fields.size());
        for (const auto& f : page.fields)
        {
            sorted.push_back(&f);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const SettingsFieldDescriptor* a, const SettingsFieldDescriptor* b) {
                      return a->order < b->order;
                  });

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
                RegisterTooltipForControl(hLabel, field.description);
                m_rightPaneTargetRects.emplace(hLabel, RECT{leftPad, y, leftPad + labelWidth, y + rowH});
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

                hCtrl = CreateWindowExW(
                    0, L"BUTTON", L"",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    ctrlX, y, ctrlWidth, rowH,
                    m_rightScrollPanel,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
                    GetModuleHandleW(nullptr), nullptr);
                if (hCtrl)
                {
                    SendMessageW(hCtrl, BM_SETCHECK,
                                 checked ? BST_CHECKED : BST_UNCHECKED, 0);
                }
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
                    const std::wstring curVal = m_settingsRegistry
                        ? m_settingsRegistry->GetValue(field.key, field.defaultValue)
                        : field.defaultValue;
                    int selIndex = 0;
                    for (size_t oi = 0; oi < field.options.size(); ++oi)
                    {
                        SendMessageW(hCtrl, CB_ADDSTRING, 0,
                                     reinterpret_cast<LPARAM>(field.options[oi].label.c_str()));
                        if (field.options[oi].value == curVal)
                        {
                            selIndex = static_cast<int>(oi);
                        }
                    }
                    SendMessageW(hCtrl, CB_SETCURSEL, static_cast<WPARAM>(selIndex), 0);
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
                    WS_EX_CLIENTEDGE, L"EDIT", curVal.c_str(),
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
                RegisterTooltipForControl(hCtrl, field.description);
                m_rightPaneTargetRects.emplace(hCtrl, RECT{ctrlX, y, ctrlX + ctrlWidth, y + rowH});

                FieldControlInfo info;
                info.key     = field.key;
                info.type    = field.type;
                info.options = field.options;
                m_controlFieldMap.emplace(ctrlId, std::move(info));
            }

            y += rowH + rowGap;
        }

        y += sectionGap;
    }

    const int contentHeight = (std::max)(y + topPad, panelHeight);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (std::max)(0, contentHeight - 1);
    si.nPage = static_cast<UINT>((std::max)(1, panelHeight));
    si.nPos = 0;
    SetScrollInfo(m_rightScrollPanel, SB_VERT, &si, TRUE);
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

        if (StartsWith(changedKey, L"appearance.theme."))
        {
            RefreshTheme();
            const UINT themeMsg = ThemePlatform::GetThemeChangedMessageId();
            SendNotifyMessageW(HWND_BROADCAST, themeMsg, 0, 0);
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

            // Built-in Settings > Plugins action: sync from plugin hub.
            if (info.key == L"settings.plugins.hub_action" && selectedValue == L"sync_now")
            {
                const std::wstring repoUrl = m_settingsRegistry->GetValue(
                    L"settings.plugins.hub_repo_url",
                    L"https://github.com/MrIvoe/Simple-Fences-Plugins.git");
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

            if (info.key == L"appearance.theme.preset_action" && m_themePlatform)
            {
                std::wstring selectedPath;
                bool completed = false;

                if (selectedValue == L"import")
                {
                    if (Win32Helpers::PromptOpenJsonFile(m_hwnd, L"Import Theme Preset", selectedPath))
                    {
                        completed = true;
                        if (!m_themePlatform->ImportCustomPreset(selectedPath))
                        {
                            Win32Helpers::ShowUserWarning(m_hwnd, L"Theme Import Failed", L"The selected preset could not be imported.");
                        }
                    }
                }
                else if (selectedValue == L"export")
                {
                    selectedPath = L"SimpleFences-theme.json";
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
        }
    }
    else if ((info.type == SettingsFieldType::String || info.type == SettingsFieldType::Int)
             && notificationCode == EN_KILLFOCUS)
    {
        wchar_t buf[1024]{};
        GetWindowTextW(hwndCtrl, buf, static_cast<int>(std::size(buf)));
        m_settingsRegistry->SetValue(info.key, buf);
        applyImmediateUiChanges(info.key);
    }
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
    switch (msg)
    {
    case WM_CREATE:
    {
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
            L"? Help",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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
            L"<",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0,
            0,
            28,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNavToggleId)),
            GetModuleHandleW(nullptr),
            nullptr);

        m_navList = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
            0,
            0,
            200,
            100,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNavId)),
            GetModuleHandleW(nullptr),
            nullptr);

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
            L"SimpleFences_SettingsRightPane",
            nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
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
        SendMessageW(m_navList, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_headerTitle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_headerSubtitle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_headerHelpButton, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetWindowSubclass(m_navList, &SettingsWindow::NavListSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

        RefreshTheme();
        PopulatePluginTabs();
        return 0;
    }
    case WM_SIZE:
    {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int navWidth = m_navCollapsed ? 64 : 280;
        const int margin = 10;
        const int topArea = kHeaderTitleHeight + kHeaderSubtitleHeight + kHeaderGap + 16;

        if (m_headerTitle)
        {
            MoveWindow(m_headerTitle,
                       navWidth + (margin * 2),
                       margin,
                       width - navWidth - (margin * 3),
                       kHeaderTitleHeight,
                       TRUE);
            SendMessageW(m_headerTitle, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_sectionFont ? m_sectionFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_headerSubtitle)
        {
            MoveWindow(m_headerSubtitle,
                       navWidth + (margin * 2),
                       margin + kHeaderTitleHeight + kHeaderGap,
                       width - navWidth - (margin * 3),
                       kHeaderSubtitleHeight,
                       TRUE);
            SendMessageW(m_headerSubtitle, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_headerHelpButton)
        {
            MoveWindow(m_headerHelpButton,
                       width - margin - 90,
                       margin + 2,
                       90,
                       28,
                       TRUE);
            SendMessageW(m_headerHelpButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_baseFont ? m_baseFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }

        if (m_navToggleButton)
        {
            MoveWindow(m_navToggleButton, margin, margin, navWidth, 30, TRUE);
            SetWindowTextW(m_navToggleButton, m_navCollapsed ? L">" : L"<");
            SendMessageW(m_navToggleButton, WM_SETFONT,
                reinterpret_cast<WPARAM>(m_navFont ? m_navFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        if (m_navList)
        {
            MoveWindow(m_navList, margin, margin + topArea, navWidth, height - (margin * 3) - topArea - kStatusHeight, TRUE);
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
        // Re-layout dynamic field controls for the selected tab.
        ShowSelectedPluginTab();
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info)
        {
            info->ptMinTrackSize.x = 640;
            info->ptMinTrackSize.y = 480;
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

        if (id == kHeaderHelpId && code == BN_CLICKED)
        {
            const LRESULT selection = m_navList ? SendMessageW(m_navList, LB_GETCURSEL, 0, 0) : -1;
            const size_t tabIndex = (selection >= 0) ? static_cast<size_t>(selection) : 0;
            MessageBoxW(hwnd, BuildTabHelpText(tabIndex).c_str(), L"Settings Help", MB_OK | MB_ICONINFORMATION);
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
        ShowSelectedPluginTab();
        SendNotifyMessageW(HWND_BROADCAST, ThemePlatform::GetThemeChangedMessageId(), 0, 0);
        return 0;
    case WM_TIMER:
        if (wParam == kRightPaneTransitionTimerId)
        {
            m_rightPaneTransitionOffset -= kRightPaneTransitionStep;
            if (m_rightPaneTransitionOffset <= 0)
            {
                m_rightPaneTransitionOffset = 0;
                KillTimer(hwnd, kRightPaneTransitionTimerId);
            }

            ApplyRightPaneTransitionLayout();

            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int navWidth = m_navCollapsed ? 64 : 280;
            const int margin = 10;
            const int topArea = kHeaderTitleHeight + kHeaderSubtitleHeight + kHeaderGap + 16;
            RECT rightPaneRect{};
            rightPaneRect.left = navWidth + (margin * 2);
            rightPaneRect.top = margin + topArea;
            rightPaneRect.right = rc.right - margin;
            rightPaneRect.bottom = rc.bottom - margin - kStatusHeight;
            if (m_rightScrollPanel && IsWindowVisible(m_rightScrollPanel))
            {
                InvalidateRect(m_rightScrollPanel, nullptr, TRUE);
            }
            else if (m_pageView && IsWindowVisible(m_pageView))
            {
                InvalidateRect(m_pageView, nullptr, TRUE);
            }
            else
            {
                InvalidateRect(hwnd, &rightPaneRect, TRUE);
            }
            return 0;
        }
        break;
    case kMsgApplyNavCollapsed:
    {
        const bool requestedCollapsed = (wParam != 0);
        m_navCollapsed = requestedCollapsed;
        PopulatePluginTabs();
        RECT rc{};
        GetClientRect(hwnd, &rc);
        SendMessageW(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

        Win32Helpers::LogInfo(
            L"Settings nav collapse apply: requested=" + std::wstring(requestedCollapsed ? L"true" : L"false") +
            L", applied=" + std::wstring(m_navCollapsed ? L"true" : L"false") +
            L", pending=" + std::wstring(m_pendingNavCollapsed ? L"true" : L"false"));
        return 0;
    }
    case WM_MEASUREITEM:
    {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure && measure->CtlID == kNavId)
        {
            measure->itemHeight = m_navCollapsed ? 44 : 40;
            measure->itemWidth = static_cast<UINT>(m_navCollapsed ? 64 : 280);
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
        break;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);
        const bool isNav = (ctrl == m_navList);

        if (ctrl == m_headerSubtitle)
        {
            SetTextColor(hdc, m_subtleTextColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
        }

        if (ctrl == m_headerTitle || ctrl == m_statusBar)
        {
            SetTextColor(hdc, m_textColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
        }

        if (ctrl == m_headerHelpButton)
        {
            SetTextColor(hdc, m_textColor);
            SetBkColor(hdc, m_windowColor);
            return reinterpret_cast<LRESULT>(m_windowBrush ? m_windowBrush : GetStockObject(WHITE_BRUSH));
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
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        DestroyThemeBrushes();
        DestroyFonts();
        if (m_tooltip)
        {
            DestroyWindow(m_tooltip);
            m_tooltip = nullptr;
        }
        m_hwnd = nullptr;
        m_navToggleButton = nullptr;
        m_headerTitle = nullptr;
        m_headerSubtitle = nullptr;
        m_headerHelpButton = nullptr;
        m_navList = nullptr;
        m_pageView = nullptr;
        m_rightScrollPanel = nullptr;
        m_statusBar = nullptr;
        m_plugins.clear();
        m_pluginTabs.clear();
        // Field control HWNDs are destroyed automatically as children of this window.
        // Just clear the tracking data.
        m_fieldControls.clear();
        m_controlFieldMap.clear();
        m_sectionCardRects.clear();
        m_rightPaneTargetRects.clear();
        m_rightPaneTextStatics.clear();
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
