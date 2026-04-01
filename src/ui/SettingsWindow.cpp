#include "ui/SettingsWindow.h"

#include "extensions/PluginSettingsRegistry.h"
#include "Win32Helpers.h"

#include <algorithm>
#include <unordered_map>
#include <windows.h>

namespace
{
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
                                   PluginSettingsRegistry* registry)
{
    m_settingsRegistry = registry;
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

    m_hwnd = CreateWindowExW(
        0,
        kClassName,
        L"SimpleFences Settings",
        WS_OVERLAPPEDWINDOW,
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

SettingsWindow::ThemeMode SettingsWindow::DetectSystemTheme() const
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
}

void SettingsWindow::RefreshTheme()
{
    const ThemeMode requested = DetectSystemTheme();

    if (requested == ThemeMode::Dark)
    {
        m_windowColor = RGB(24, 24, 24);
        m_surfaceColor = RGB(36, 36, 36);
        m_textColor = RGB(232, 232, 232);
    }
    else
    {
        m_windowColor = GetSysColor(COLOR_WINDOW);
        m_surfaceColor = GetSysColor(COLOR_WINDOW);
        m_textColor = GetSysColor(COLOR_WINDOWTEXT);
    }

    m_themeMode = requested;

    DestroyThemeBrushes();
    m_windowBrush = CreateSolidBrush(m_windowColor);
    m_surfaceBrush = CreateSolidBrush(m_surfaceColor);

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

    SendMessageW(m_navList, LB_RESETCONTENT, 0, 0);

    for (const auto& tab : m_pluginTabs)
    {
        std::wstring label = tab.iconGlyph + L"  " + tab.title;
        SendMessageW(m_navList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }

    SendMessageW(m_navList, LB_SETCURSEL, 0, 0);
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

    // Destroy any existing field controls from the previous selection.
    ClearFieldControls();

    if (hasFields)
    {
        // Hide the read-only EDIT; render interactive controls instead.
        ShowWindow(m_pageView, SW_HIDE);

        RECT clientRect{};
        GetClientRect(m_hwnd, &clientRect);
        const int width = clientRect.right - clientRect.left;
        const int navWidth = 250;
        const int margin   = 10;
        const int rightX   = navWidth + (margin * 2);
        const int rightY   = margin;
        const int rightW   = width - navWidth - (margin * 3);

        PopulateFieldControls(tabIndex, rightX, rightY, rightW);
    }
    else
    {
        // Show the read-only EDIT with text content.
        ShowWindow(m_pageView, SW_SHOW);
        SetWindowTextW(m_pageView, BuildSelectedTabContent(tabIndex).c_str());
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
    text += L"Phase 0.0.010 settings shell is active.\r\n";
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
        return L"Theme\r\n\r\n"
               L"- Current app settings host theme follows system preference.\r\n"
               L"- Active theme: " + themeText + L"\r\n"
               L"- Suggested future toggles: force light, force dark, use system accent color.";
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
    for (HWND hwnd : m_fieldControls)
    {
        if (hwnd)
        {
            DestroyWindow(hwnd);
        }
    }
    m_fieldControls.clear();
    m_controlFieldMap.clear();
    // Reset ID counter so IDs don't climb unboundedly across tab switches.
    m_nextControlId = 2000;
}

void SettingsWindow::PopulateFieldControls(size_t tabIndex, int rightX, int rightY, int rightW)
{
    const HFONT hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const int   rowH        = 28;
    const int   rowGap      = 6;
    const int   sectionGap  = 18;
    const int   labelWidth  = 220;
    const int   ctrlWidth   = 200;
    const int   ctrlGap     = 8;

    int y = rightY;

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

        // --- Section header (page title) ----------------------------------------
        HWND hHeader = CreateWindowExW(
            0, L"STATIC", page.title.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            rightX, y, rightW, rowH,
            m_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hHeader)
        {
            SendMessageW(hHeader, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            m_fieldControls.push_back(hHeader);
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
                rightX, y, labelWidth, rowH,
                m_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (hLabel)
            {
                SendMessageW(hLabel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                m_fieldControls.push_back(hLabel);
            }

            // Sub-description (optional, shown on the next line)
            if (!field.description.empty())
            {
                // we'll draw the description below the control row later; skip for now
            }

            const int ctrlX  = rightX + labelWidth + ctrlGap;
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
                    m_hwnd,
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
                    m_hwnd,
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
                    m_hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
                    GetModuleHandleW(nullptr), nullptr);
            }

            if (hCtrl)
            {
                SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                m_fieldControls.push_back(hCtrl);

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

    if (info.type == SettingsFieldType::Bool && notificationCode == BN_CLICKED)
    {
        const bool checked = (SendMessageW(hwndCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_settingsRegistry->SetValue(info.key, checked ? L"true" : L"false");
    }
    else if (info.type == SettingsFieldType::Enum && notificationCode == CBN_SELCHANGE)
    {
        const LRESULT sel = SendMessageW(hwndCtrl, CB_GETCURSEL, 0, 0);
        if (sel >= 0 && static_cast<size_t>(sel) < info.options.size())
        {
            m_settingsRegistry->SetValue(info.key, info.options[static_cast<size_t>(sel)].value);
        }
    }
    else if ((info.type == SettingsFieldType::String || info.type == SettingsFieldType::Int)
             && notificationCode == EN_KILLFOCUS)
    {
        wchar_t buf[1024]{};
        GetWindowTextW(hwndCtrl, buf, static_cast<int>(std::size(buf)));
        m_settingsRegistry->SetValue(info.key, buf);
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
        m_navList = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
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

        SendMessageW(m_pageView, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(m_navList, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

        RefreshTheme();
        PopulatePluginTabs();
        return 0;
    }
    case WM_SIZE:
    {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int navWidth = 250;
        const int margin = 10;

        if (m_navList)
        {
            MoveWindow(m_navList, margin, margin, navWidth, height - (margin * 2), TRUE);
        }
        if (m_pageView)
        {
            MoveWindow(m_pageView, navWidth + (margin * 2), margin, width - navWidth - (margin * 3), height - (margin * 2), TRUE);
        }
        return 0;
    }
    case WM_COMMAND:
    {
        const int id   = LOWORD(wParam);
        const int code = HIWORD(wParam);
        HWND hwndCtrl  = reinterpret_cast<HWND>(lParam);

        if (id == kNavId && code == LBN_SELCHANGE)
        {
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
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        if (m_themeMode != ThemeMode::Dark)
        {
            break;
        }

        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, m_textColor);
        SetBkColor(hdc, m_surfaceColor);
        return reinterpret_cast<LRESULT>(m_surfaceBrush ? m_surfaceBrush : GetStockObject(DC_BRUSH));
    }
    case WM_ERASEBKGND:
    {
        if (m_themeMode != ThemeMode::Dark || !m_windowBrush)
        {
            break;
        }

        RECT rect{};
        GetClientRect(hwnd, &rect);
        FillRect(reinterpret_cast<HDC>(wParam), &rect, m_windowBrush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        DestroyThemeBrushes();
        m_hwnd = nullptr;
        m_navList = nullptr;
        m_pageView = nullptr;
        m_plugins.clear();
        m_pluginTabs.clear();
        // Field control HWNDs are destroyed automatically as children of this window.
        // Just clear the tracking data.
        m_fieldControls.clear();
        m_controlFieldMap.clear();
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
