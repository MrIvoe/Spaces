#pragma once
#include <windows.h>

class FenceManager;

class ZOrderCoordinator {
public:
    ZOrderCoordinator(HINSTANCE instance, FenceManager* manager);
    ~ZOrderCoordinator();

    bool Initialize();
    void RequestSync();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnTimer();

private:
    HINSTANCE m_instance{};
    FenceManager* m_manager{};
    HWND m_hwnd{};
};
