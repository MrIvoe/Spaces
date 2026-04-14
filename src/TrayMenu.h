#pragma once

#include "Win32Helpers.h"

#include <windows.h>

#include <memory>
#include <string>
#include <unordered_map>

class App;

enum class TrayNotifyMode
{
    Legacy,
    Version4
};

class TrayMenu
{
public:
    explicit TrayMenu(App* app);
    ~TrayMenu();

    bool Create(HINSTANCE hInstance);
    void Destroy();
    void ShowContextMenu(POINT pt);
    void RefreshTooltipText();

    // Diagnostic: Verify tray icon is registered and functional
    bool IsTrayIconValid() const;
    
    // Diagnostic: Get human-readable status for logging/debugging
    std::wstring GetDiagnosticStatus() const;

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    App* m_app;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATA m_nid{};
    UINT m_taskbarCreatedMessage = 0;
    TrayNotifyMode m_notifyMode = TrayNotifyMode::Legacy;
    std::unordered_map<UINT, std::wstring> m_commandByMenuId;
    std::unordered_map<UINT, Win32Helpers::PopupMenuItemVisual> m_menuVisualByMenuId;
};
