#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

class FenceManager;

class TrayIcon {
public:
    TrayIcon(HINSTANCE instance, FenceManager* manager);
    ~TrayIcon();

    bool Initialize();
    void ShowNotification(const std::wstring& title, const std::wstring& message, bool error);
    bool GetShowInfoNotifications() const { return m_showInfoNotifications; }
    void SetShowInfoNotifications(bool enabled);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowMenu();

private:
    HINSTANCE m_instance{};
    FenceManager* m_manager{};
    HWND m_hwnd{};
    NOTIFYICONDATAW m_nid{};
    ULONGLONG m_lastInfoTick{};
    std::wstring m_lastInfoTitle;
    std::wstring m_lastInfoMessage;
    bool m_showInfoNotifications{true};
};
