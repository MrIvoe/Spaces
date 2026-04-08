#include "ZOrderCoordinator.h"
#include "SpaceManager.h"

namespace {
constexpr wchar_t kClassName[] = L"IVOESpacesZOrderCoordinator";
constexpr UINT_PTR kSyncTimer = 1;
constexpr UINT kSyncIntervalMs = 750;
}

ZOrderCoordinator::ZOrderCoordinator(HINSTANCE instance, SpaceManager* manager)
    : m_instance(instance), m_manager(manager) {
}

ZOrderCoordinator::~ZOrderCoordinator() {
    if (m_hwnd) {
        KillTimer(m_hwnd, kSyncTimer);
        DestroyWindow(m_hwnd);
    }
}

bool ZOrderCoordinator::Initialize() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = ZOrderCoordinator::WndProc;
    wc.hInstance = m_instance;
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);

    m_hwnd = CreateWindowExW(
        0, kClassName, L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, m_instance, this);

    if (!m_hwnd) {
        return false;
    }

    SetTimer(m_hwnd, kSyncTimer, kSyncIntervalMs, nullptr);
    return true;
}

void ZOrderCoordinator::RequestSync() {
    if (m_manager) {
        m_manager->MaintainDesktopPlacement();
    }
}

void ZOrderCoordinator::OnTimer() {
    RequestSync();
}

LRESULT CALLBACK ZOrderCoordinator::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ZOrderCoordinator* self = reinterpret_cast<ZOrderCoordinator*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!self && msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<ZOrderCoordinator*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    if (msg == WM_TIMER && wParam == kSyncTimer) {
        self->OnTimer();
        return 0;
    }

    if (msg == WM_DISPLAYCHANGE || msg == WM_SETTINGCHANGE || msg == WM_WINDOWPOSCHANGED) {
        self->RequestSync();
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
