#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "Win32Helpers.h"
#include "Models.h"

class FenceManager;
class ThemePlatform;

struct EffectiveFencePolicy
{
    bool rollupWhenNotHovered = false;
    bool transparentWhenNotHovered = false;
    bool labelsOnHover = true;
    int iconTileSize = 56;
};

class FenceWindow
{
public:
    FenceWindow(FenceManager* manager, const FenceModel& model, const ThemePlatform* themePlatform);
    ~FenceWindow();

    bool Create(HWND parent = nullptr);
    void Show();
    void Destroy();
    void UpdateModel(const FenceModel& model);
    void SetItems(const std::vector<FenceItem>& items);

    HWND GetHwnd() const;
    const std::wstring& GetFenceId() const;
    const FenceModel& GetModel() const;

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

    void ShowSettingsPanel();
    EffectiveFencePolicy ResolveEffectivePolicy() const;
    int GetItemAtPosition(int x, int y) const;
    void ApplyIdleVisualState();
    void ExecuteItem(int itemIndex);
    bool InitializeImageList();

    static LRESULT CALLBACK SettingsPanelWndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT SettingsPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    FenceManager* m_manager;
    FenceModel m_model;
    const ThemePlatform* m_themePlatform = nullptr;
    std::vector<FenceItem> m_items;
    HWND m_hwnd = nullptr;
    HIMAGELIST m_imageList = nullptr;  // System image list for file icons
    bool m_dragging = false;
    POINT m_dragStart{};
    RECT m_windowStart{};
    int m_selectedItem = -1;
    bool m_mouseInside = false;
    bool m_isRolledUp = false;
    int m_expandedHeight = 0;
    HWND m_settingsPanel = nullptr;
    std::unordered_map<UINT, Win32Helpers::PopupMenuItemVisual> m_menuVisuals;
    std::unordered_map<UINT, std::wstring> m_menuCommands;
};
