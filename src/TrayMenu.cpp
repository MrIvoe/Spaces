#include "TrayMenu.h"
#include "App.h"
#include "FenceManager.h"
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

static constexpr UINT WMAPP_TRAYICON = (WM_APP + 1);
static constexpr UINT ID_TRAY_NEW_FENCE = 1001;
static constexpr UINT ID_TRAY_EXIT = 1002;

TrayMenu::TrayMenu(App* app) : m_app(app)
{
}

TrayMenu::~TrayMenu()
{
    Destroy();
}

bool TrayMenu::Create(HINSTANCE hInstance)
{
    // Create message-only window
    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayMenu::WndProcStatic;
    wc.lpszClassName = L"SimpleFences_TrayWindow";
    wc.hInstance = hInstance;

    if (!RegisterClassW(&wc))
        return false;

    m_hwnd = CreateWindowW(
        wc.lpszClassName,
        L"",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        this);

    if (!m_hwnd)
        return false;

    // Add notify icon
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    m_nid.uCallbackMessage = WMAPP_TRAYICON;
    m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpyW(m_nid.szTip, L"SimpleFences");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid))
        return false;

    return true;
}

void TrayMenu::Destroy()
{
    if (m_hwnd)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void TrayMenu::ShowContextMenu(POINT pt)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_NEW_FENCE, L"New Fence");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, m_hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd == ID_TRAY_NEW_FENCE && m_app)
    {
        m_app->CreateFenceNearCursor();
    }
    else if (cmd == ID_TRAY_EXIT && m_app)
    {
        m_app->Exit();
    }
}

LRESULT CALLBACK TrayMenu::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayMenu* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<TrayMenu*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = reinterpret_cast<TrayMenu*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis)
        return pThis->WndProc(hwnd, msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayMenu::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WMAPP_TRAYICON)
    {
        if (lParam == WM_RBUTTONUP)
        {
            POINT pt{};
            GetCursorPos(&pt);
            ShowContextMenu(pt);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
