#pragma once
#include <windows.h>

class SpaceManager;

class ShellChangeWatcher {
public:
    ShellChangeWatcher(HINSTANCE instance, SpaceManager* manager);
    ~ShellChangeWatcher();

    bool Initialize();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE m_instance{};
    SpaceManager* m_manager{};
    HWND m_hwnd{};
    ULONG m_notifyId{};
};
