#pragma once
#include <windows.h>

class SpaceManager;

class ZOrderCoordinator {
public:
    ZOrderCoordinator(HINSTANCE instance, SpaceManager* manager);
    ~ZOrderCoordinator();

    bool Initialize();
    void RequestSync();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnTimer();

private:
    HINSTANCE m_instance{};
    SpaceManager* m_manager{};
    HWND m_hwnd{};
};
