#pragma once

#include "core/KernelViews.h"
#include "core/ThemePlatform.h"
#include "extensions/SettingsSchema.h"

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <windows.h>

class PluginSettingsRegistry;

class SettingsWindow
{
public:
    // registry may be nullptr; when supplied, control values are read/written
    // through it so that changes persist to settings.json.
    bool ShowScaffold(const std::vector<SettingsPageView>& pages,
                      const std::vector<PluginStatusView>& plugins,
                      PluginSettingsRegistry* registry,
                      const ThemePlatform* themePlatform);

private:
    struct UiPage
    {
        std::wstring pageId;
        std::wstring pluginId;
        std::wstring title;
        std::wstring content;                       // text used when no fields are declared
        std::vector<SettingsFieldDescriptor> fields; // interactive controls
    };

    struct PluginTab
    {
        std::wstring pluginId;
        std::wstring iconGlyph;
        std::wstring title;
        std::vector<size_t> pageIndexes;
    };

    // Control record for a dynamically created field control
    struct FieldControlInfo
    {
        std::wstring key;
        SettingsFieldType type;
        std::vector<SettingsEnumOption> options; // used for Enum controls
    };

    bool EnsureWindow();
    void RefreshTheme();
    void DestroyThemeBrushes();
    void DestroyFonts();
    void BuildPluginTabs(const std::vector<PluginStatusView>& plugins);
    void PopulatePluginTabs();
    void ShowSelectedPluginTab();
    std::wstring BuildSelectedTabContent(size_t tabIndex) const;
    std::vector<UiPage> BuildPages(const std::vector<SettingsPageView>& pages,
                                   const std::vector<PluginStatusView>& plugins) const;
    std::wstring ResolvePluginDisplayName(const std::wstring& pluginId,
                                          const std::vector<PluginStatusView>& plugins) const;
    std::wstring ResolvePluginIcon(const PluginStatusView& plugin) const;

    std::wstring BuildGeneralContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildPluginsContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildDiagnosticsContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildGenericPageContent(const SettingsPageView& page) const;
    std::wstring BuildPluginOverviewContent(const PluginStatusView& plugin) const;

    // Interactive field controls
    void ClearFieldControls();
    void PopulateFieldControls(size_t tabIndex, int rightX, int rightY, int rightW);
    void HandleFieldControlChange(int ctrlId, int notificationCode, HWND hwndCtrl);
    void RegisterTooltipForControl(HWND control, const std::wstring& tipText);
    void UpdateShellHeaderAndStatus(size_t tabIndex);
    std::wstring BuildTabHelpText(size_t tabIndex) const;
    void DrawNavItem(const DRAWITEMSTRUCT* drawInfo);
    static LRESULT CALLBACK NavListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR subclassId, DWORD_PTR refData);
    // Right-pane scroll panel
    static LRESULT CALLBACK ScrollPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ApplyScrollPanelScroll(int delta);
    LRESULT DrawScrollPanelBkgnd(HDC hdc);

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    HWND m_navToggleButton = nullptr;
    HWND m_headerTitle = nullptr;
    HWND m_headerSubtitle = nullptr;
    HWND m_headerHelpButton = nullptr;
    HWND m_navList = nullptr;
    HWND m_pageView = nullptr;          // multiline EDIT – used for text/overview pages
    HWND m_statusBar = nullptr;
    std::vector<UiPage> m_pages;
    std::vector<PluginStatusView> m_plugins;
    std::vector<PluginTab> m_pluginTabs;

    // Settings persistence
    PluginSettingsRegistry* m_settingsRegistry = nullptr;
    const ThemePlatform* m_themePlatform = nullptr;

    // Dynamically created field controls (children of m_hwnd, right pane)
    std::vector<HWND>                         m_fieldControls;
    std::unordered_map<int, FieldControlInfo> m_controlFieldMap;
    std::vector<RECT>                         m_sectionCardRects;
    std::unordered_set<HWND>                  m_rightPaneTextStatics;
    int                                        m_nextControlId = 2000;

    // Right-pane scroll panel (parent of all field controls)
    HWND m_rightScrollPanel = nullptr;

    // Use system-wide palette types from ThemePlatform.h
    ThemeMode m_themeMode = ThemeMode::Light;
    ThemeStyle m_themeStyle = ThemeStyle::System;
    COLORREF m_windowColor = RGB(255, 255, 255);
    COLORREF m_surfaceColor = RGB(255, 255, 255);
    COLORREF m_navColor = RGB(245, 245, 245);
    COLORREF m_textColor = RGB(0, 0, 0);
    COLORREF m_subtleTextColor = RGB(95, 95, 95);
    COLORREF m_accentColor = RGB(70, 120, 220);
    HBRUSH m_windowBrush = nullptr;
    HBRUSH m_surfaceBrush = nullptr;
    HBRUSH m_navBrush = nullptr;

    HFONT m_baseFont = nullptr;
    HFONT m_sectionFont = nullptr;
    HFONT m_navFont = nullptr;
    HWND m_tooltip = nullptr;

    bool m_navCollapsed = false;
    int m_navHoverIndex = -1;
    DWORD m_lastNavToggleTick = 0;
    bool m_pendingNavCollapsed = false;

    static constexpr int kNavToggleId = 100;
    static constexpr int kNavId  = 101;
    static constexpr int kPageId = 102;
    static constexpr int kHeaderTitleId = 103;
    static constexpr int kHeaderSubtitleId = 104;
    static constexpr int kStatusId = 105;
    static constexpr int kHeaderHelpId = 106;
    static constexpr UINT kMsgApplyNavCollapsed = WM_APP + 41;
    static constexpr DWORD kNavToggleDebounceMs = 150;
};
