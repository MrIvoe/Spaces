#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "Win32Helpers.h"
#include "Models.h"

class SpaceManager;
class ThemePlatform;

struct EffectiveSpacePolicy
{
    bool rollupWhenNotHovered = false;
    bool transparentWhenNotHovered = false;
    bool labelsOnHover = true;
    int iconTileSize = 56;
};

class SpaceWindow
{
public:
    SpaceWindow(SpaceManager* manager, const SpaceModel& model, const ThemePlatform* themePlatform);
    ~SpaceWindow();

    bool Create(HWND parent = nullptr);
    void Show();
    void Destroy();
    void UpdateModel(const SpaceModel& model);
    void SetItems(const std::vector<SpaceItem>& items);

    HWND GetHwnd() const;
    const std::wstring& GetSpaceId() const;
    const SpaceModel& GetModel() const;

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y, WPARAM flags);
    void OnLButtonUp();
    void OnLButtonDblClk(int x, int y);
    void OnContextMenu(int x, int y);
    void OnDropFiles(HDROP hDrop);
    void OnMove(int x, int y);
    void OnSize(int width, int height);
    void ShowCommandPalette();

    void ShowSettingsPanel();
    EffectiveSpacePolicy ResolveEffectivePolicy() const;
    int GetItemAtPosition(int x, int y) const;
    bool ApplyIdleVisualState();
    void ExecuteItem(int itemIndex);
    bool InitializeImageList();
    std::wstring NewCorrelationId(const wchar_t* action) const;
    void TraceDebug(const std::wstring& message, const std::wstring& correlationId = L"") const;

    static LRESULT CALLBACK SettingsPanelWndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT SettingsPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    SpaceManager* m_manager;
    SpaceModel m_model;
    const ThemePlatform* m_themePlatform = nullptr;
    std::vector<SpaceItem> m_items;
    HWND m_hwnd = nullptr;
    HIMAGELIST m_imageList = nullptr;  // System image list for file icons
    bool m_dragging = false;
    POINT m_dragStart{};
    RECT m_windowStart{};
    bool m_itemDragPending = false;
    bool m_itemDragActive = false;
    int m_itemDragSourceIndex = -1;
    int m_itemDragTargetIndex = -1;
    int m_selectedItem = -1;
    bool m_mouseInside = false;
    bool m_isRolledUp = false;
    int m_expandedHeight = 0;
    bool m_lastLoggedTransparent = false;
    bool m_hasLastLoggedTransparent = false;
    BYTE m_currentAlpha = 255;
    BYTE m_targetAlpha = 255;
    bool m_alphaInitialized = false;
    bool m_introAnimActive = false;
    DWORD m_introAnimStartTick = 0;
    float m_introAnimProgress = 1.0f;
    BYTE m_introAnimAlpha = 255;
    int m_loadingSpinnerFrame = 0;
    unsigned int m_idleStateEvalCount = 0;
    std::wstring m_activeCorrelationId;
    std::wstring m_geometryCorrelationId;
    bool m_internalIdleResize = false;
    int m_titleBarHoverButton = 0;
    HWND m_settingsPanel = nullptr;
    std::unordered_map<UINT, Win32Helpers::PopupMenuItemVisual> m_menuVisuals;
    std::unordered_map<UINT, std::wstring> m_menuCommands;
};
