#include "ShellChangeWatcher.h"
#include "FenceManager.h"
#include <shlobj.h>

namespace {
constexpr wchar_t kClassName[] = L"IVOEFencesShellChangeWatcher";
constexpr UINT kShellNotifyMsg = WM_APP + 20;
}

ShellChangeWatcher::ShellChangeWatcher(HINSTANCE instance, FenceManager* manager)
    : m_instance(instance), m_manager(manager) {
}

ShellChangeWatcher::~ShellChangeWatcher() {
    if (m_notifyId != 0) {
        SHChangeNotifyDeregister(m_notifyId);
        m_notifyId = 0;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool ShellChangeWatcher::Initialize() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = ShellChangeWatcher::WndProc;
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

    SHChangeNotifyEntry entry{};
    entry.pidl = nullptr;
    entry.fRecursive = TRUE;

    m_notifyId = SHChangeNotifyRegister(
        m_hwnd,
        SHCNRF_ShellLevel | SHCNRF_InterruptLevel | SHCNRF_NewDelivery,
        SHCNE_ALLEVENTS,
        kShellNotifyMsg,
        1,
        &entry);

    return m_notifyId != 0;
}

LRESULT CALLBACK ShellChangeWatcher::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ShellChangeWatcher* self = reinterpret_cast<ShellChangeWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!self && msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<ShellChangeWatcher*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    if (msg == kShellNotifyMsg && self->m_manager) {
        self->m_manager->OnShellChanged();
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
