#pragma once
#include <windows.h>
#include <memory>

class App;

class TrayMenu
{
public:
    explicit TrayMenu(App* app);
    ~TrayMenu();

    bool Create(HINSTANCE hInstance);
    void Destroy();
    void ShowContextMenu(POINT pt);

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    App* m_app;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATA m_nid{};
};
