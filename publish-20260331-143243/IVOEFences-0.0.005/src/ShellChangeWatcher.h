#pragma once
#include <windows.h>

class FenceManager;

class ShellChangeWatcher {
public:
    ShellChangeWatcher(HINSTANCE instance, FenceManager* manager);
    ~ShellChangeWatcher();

    bool Initialize();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE m_instance{};
    FenceManager* m_manager{};
    HWND m_hwnd{};
    ULONG m_notifyId{};
};
