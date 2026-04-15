#pragma once

#include "core/KernelViews.h"
#include "core/ThemePlatform.h"
#include "extensions/SettingsSchema.h"

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <commctrl.h>

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
        std::wstring iconKey;
        std::wstring iconGlyph;
        std::wstring iconAsset;
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

    struct FieldControlLayout
    {
        HWND hwnd = nullptr;
        int baseX = 0;
        int baseY = 0;
        int width = 0;
        int height = 0;
        bool stretchToRight = false;
        int rightMargin = 0;
    };

    bool EnsureWindow();
    void RefreshTheme();
    void DestroyThemeBrushes();
    void DestroyFonts();
    void BuildPluginTabs(const std::vector<PluginStatusView>& plugins);
    void ApplyWindowTranslucency();
    void PopulatePluginTabs();
    void ShowSelectedPluginTab();
    std::wstring BuildSelectedTabContent(size_t tabIndex) const;
    std::vector<UiPage> BuildPages(const std::vector<SettingsPageView>& pages,
                                   const std::vector<PluginStatusView>& plugins) const;
    std::wstring ResolvePluginDisplayName(const std::wstring& pluginId,
                                          const std::vector<PluginStatusView>& plugins) const;
    std::wstring ResolvePluginIconKey(const PluginStatusView& plugin) const;
    std::wstring BuildGeneralContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildPluginsContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildMarketplaceDiscoverContent() const;
    std::wstring BuildMarketplaceInstalledContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildMarketplaceUpdatesContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildMarketplaceDisabledContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildDiagnosticsContent(const std::vector<PluginStatusView>& plugins) const;
    std::wstring BuildGenericPageContent(const SettingsPageView& page) const;
    std::wstring BuildPluginOverviewContent(const PluginStatusView& plugin) const;

    // Interactive field controls
    void ClearFieldControls();
    void CommitPendingTextFieldEdits();
    void PopulateFieldControls(size_t tabIndex, int rightX, int rightY, int rightW);
    bool IsFieldVisibleInBasicMode(const SettingsFieldDescriptor& field) const;
    void RelayoutScrollPanelChildren();
    void HandleFieldControlChange(int ctrlId, int notificationCode, HWND hwndCtrl);
    void RegisterTooltipForControl(HWND control, const std::wstring& tipText);
    void UpdateShellHeaderAndStatus(size_t tabIndex);
    std::wstring BuildTabHelpText(size_t tabIndex) const;
    void ShowMenuBarPopup(int buttonId);
    void HandleMenuBarCommand(UINT commandId);
    bool SelectTabByPluginId(const std::wstring& pluginId);
    HWND FindFieldControlByKey(const std::wstring& key) const;
    void FocusPreferredSearchField();
    bool MatchesContentSearch(const SettingsFieldDescriptor& field) const;
    bool MatchesContentChipFilter(const SettingsFieldDescriptor& field) const;
    bool IsBuiltinPluginsTabSelected() const;
    bool ShouldShowMarketplacePage(const std::wstring& pageId) const;
    void PopulatePluginTree(size_t tabIndex);
    void ClearPluginTree();
    bool ShouldShowPluginTree() const;
    void DrawNavItem(const DRAWITEMSTRUCT* drawInfo);
    void DrawToggleControl(const DRAWITEMSTRUCT* drawInfo);
    void DrawShellButton(const DRAWITEMSTRUCT* drawInfo);
    void DrawFieldSurfaceFrame(HWND control);
    void UpdateMarketplaceStatusState(size_t tabIndex);
    void InvalidateNavTransitionRegion(int oldNavWidth, int newNavWidth);
    void StartNavCollapseAnimation(bool requestedCollapsed);
    void StepNavCollapseAnimation();
    static LRESULT CALLBACK FieldSurfaceSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR subclassId, DWORD_PTR refData);
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
    HWND m_menuFileButton = nullptr;
    HWND m_menuEditButton = nullptr;
    HWND m_menuViewButton = nullptr;
    HWND m_menuPluginsButton = nullptr;
    HWND m_contentSearchEdit = nullptr;
    HWND m_chipAllButton = nullptr;
    HWND m_chipToggleButton = nullptr;
    HWND m_chipChoiceButton = nullptr;
    HWND m_chipTextButton = nullptr;
    HWND m_modeBasicButton = nullptr;
    HWND m_modeAdvancedButton = nullptr;
    HWND m_marketplaceDiscoverTabButton = nullptr;
    HWND m_marketplaceInstalledTabButton = nullptr;
    HWND m_pluginTreeView = nullptr;
    HWND m_headerTitle = nullptr;
    HWND m_headerSubtitle = nullptr;
    HWND m_headerHelpButton = nullptr;
    // HWND m_navList = nullptr; // replaced by VirtualNavList
    class VirtualNavList* m_navList = nullptr;
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
    std::vector<FieldControlLayout>           m_fieldControlLayouts;
    std::unordered_map<int, FieldControlInfo> m_controlFieldMap;
    std::unordered_map<HWND, SettingsFieldType> m_fieldSurfaceTypes;
    std::vector<RECT>                         m_sectionCardRects;
    std::unordered_set<HWND>                  m_rightPaneTextStatics;
    std::unordered_set<HWND>                  m_sectionHeaderStatics;
    int                                       m_nextControlId = 2000;
    int                                       m_rightPaneScrollY = 0;
    int                                       m_rightPaneContentHeight = 0;

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
    HFONT m_iconFont = nullptr;
    HWND m_tooltip = nullptr;

    bool m_navCollapsed = false;
    int m_navAnimatedWidth = 280;
    int m_navAnimationStartWidth = 280;
    int m_navAnimationTargetWidth = 280;
    DWORD m_navAnimationStartTick = 0;
    int m_navAnimationDurationMs = 180;
    bool m_navAnimating = false;
    int m_navHoverIndex = -1;
    DWORD m_lastNavToggleTick = 0;
    bool m_pendingNavCollapsed = false;
    bool m_marketplaceSpinnerActive = false;
    int m_marketplaceSpinnerFrame = 0;
    std::wstring m_marketplaceStatusChip;
    std::wstring m_contentSearchQuery;
    int m_activeContentChip = 0;
    int m_marketplaceSubTab = 0; // 0=Discover, 1=Installed
    bool m_showAdvancedSettings = false;

    static constexpr int kNavToggleId = 100;
    static constexpr int kNavId  = 101;
    static constexpr int kPageId = 102;
    static constexpr int kHeaderTitleId = 103;
    static constexpr int kHeaderSubtitleId = 104;
    static constexpr int kStatusId = 105;
    static constexpr int kHeaderHelpId = 106;
    static constexpr int kMenuFileId = 107;
    static constexpr int kMenuEditId = 108;
    static constexpr int kMenuViewId = 109;
    static constexpr int kMenuPluginsId = 110;
    static constexpr int kContentSearchId = 111;
    static constexpr int kChipAllId = 112;
    static constexpr int kChipToggleId = 113;
    static constexpr int kChipChoiceId = 114;
    static constexpr int kChipTextId = 115;
    static constexpr int kModeBasicId = 116;
    static constexpr int kModeAdvancedId = 117;
    static constexpr int kMarketplaceDiscoverTabId = 118;
    static constexpr int kMarketplaceInstalledTabId = 119;
    static constexpr int kPluginTreeViewId = 120;
    static constexpr UINT kMenuCmdSaveSettings = 5201;
    static constexpr UINT kMenuCmdCloseSettings = 5202;
    static constexpr UINT kMenuCmdUndo = 5211;
    static constexpr UINT kMenuCmdRedo = 5212;
    static constexpr UINT kMenuCmdFind = 5213;
    static constexpr UINT kMenuCmdToggleSidebar = 5221;
    static constexpr UINT kMenuCmdReloadCurrent = 5222;
    static constexpr UINT kMenuCmdOpenPlugins = 5231;
    static constexpr UINT kMenuCmdCheckUpdates = 5232;
    static constexpr UINT kMenuCmdKeyboardShortcuts = 5241;
    static constexpr UINT kMenuCmdAbout = 5242;
    static constexpr int kHotkeyNextSectionId = 6101;
    static constexpr int kHotkeyPrevSectionId = 6102;
    static constexpr int kHotkeyFocusSearchId = 6103;
    static constexpr int kHotkeyFocusNavId = 6104;
    static constexpr int kHotkeyHelpId = 6105;
    static constexpr UINT kMsgApplyNavCollapsed = WM_APP + 41;
    static constexpr UINT_PTR kNavAnimationTimerId = 4101;
    static constexpr UINT_PTR kMarketplaceSpinnerTimerId = 4102;
    static constexpr int kNavExpandedWidth = 280;
    static constexpr int kNavCollapsedWidth = 64;
    static constexpr DWORD kNavToggleDebounceMs = 150;
};
