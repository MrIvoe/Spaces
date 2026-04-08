#include "TrayIcon.h"
#include "AppVersion.h"
#include "SpaceManager.h"
#include <shellapi.h>
#include <shlobj.h>

namespace {
constexpr UINT WMAPP_TRAY = WM_APP + 1;
constexpr UINT ID_TRAY_NEW_SPACE = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY_OPEN_FOLDER = 1003;
constexpr UINT ID_TRAY_SHOW_INFO_NOTIFICATIONS = 1004;
constexpr UINT ID_TRAY_NEW_PORTAL_SPACE = 1005;
constexpr UINT ID_DROP_POLICY_MOVE = 1101;
constexpr UINT ID_DROP_POLICY_COPY = 1102;
constexpr UINT ID_DROP_POLICY_PROMPT = 1103;
constexpr wchar_t kTrayClassName[] = L"IVOESpacesTrayWindow";
constexpr ULONGLONG kInfoNotifyMinIntervalMs = 5000;

std::wstring PickFolderPath(HWND owner) {
    BROWSEINFOW browseInfo{};
    browseInfo.hwndOwner = owner;
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    browseInfo.lpszTitle = L"Select a folder for Portal Space";

    PIDLIST_ABSOLUTE itemList = SHBrowseForFolderW(&browseInfo);
    if (itemList == nullptr) {
        return {};
    }

    wchar_t path[MAX_PATH]{};
    const BOOL ok = SHGetPathFromIDListW(itemList, path);
    CoTaskMemFree(itemList);
    if (!ok) {
        return {};
    }

    return path;
}
}

TrayIcon::TrayIcon(HINSTANCE instance, SpaceManager* manager)
    : m_instance(instance), m_manager(manager) {
}

TrayIcon::~TrayIcon() {
    if (m_nid.cbSize) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }
}

bool TrayIcon::Initialize() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayIcon::WndProc;
    wc.hInstance = m_instance;
    wc.lpszClassName = kTrayClassName;
    RegisterClassW(&wc);

    m_hwnd = CreateWindowExW(
        0, kTrayClassName, L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, m_instance, this);

    if (!m_hwnd) {
        return false;
    }

    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WMAPP_TRAY;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    std::wstring trayTip = std::wstring(L"IVOE Spaces ") + AppVersion::kCurrent;
    lstrcpynW(m_nid.szTip, trayTip.c_str(), static_cast<int>(std::size(m_nid.szTip)));
    m_showInfoNotifications = m_manager->GetShowInfoNotifications();

    return Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE;
}

void TrayIcon::SetShowInfoNotifications(bool enabled) {
    m_showInfoNotifications = enabled;
    m_manager->SetShowInfoNotifications(enabled);
}

void TrayIcon::ShowNotification(const std::wstring& title, const std::wstring& message, bool error) {
    if (!m_nid.cbSize) {
        return;
    }

    if (!error && !m_showInfoNotifications) {
        return;
    }

    if (!error) {
        ULONGLONG now = GetTickCount64();
        bool duplicate = (title == m_lastInfoTitle && message == m_lastInfoMessage);
        if ((now - m_lastInfoTick) < kInfoNotifyMinIntervalMs || duplicate) {
            return;
        }

        m_lastInfoTick = now;
        m_lastInfoTitle = title;
        m_lastInfoMessage = message;
    }

    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = error ? NIIF_ERROR : NIIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::ShowMenu() {
    HMENU menu = CreatePopupMenu();
    HMENU settingsMenu = CreatePopupMenu();
    HMENU policyMenu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, ID_TRAY_NEW_SPACE, L"New Space");
    AppendMenuW(menu, MF_STRING, ID_TRAY_NEW_PORTAL_SPACE, L"New Portal Space...");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_FOLDER, L"Open Backing Folder");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(policyMenu, MF_STRING, ID_DROP_POLICY_MOVE, L"Move");
    AppendMenuW(policyMenu, MF_STRING, ID_DROP_POLICY_COPY, L"Copy");
    AppendMenuW(policyMenu, MF_STRING, ID_DROP_POLICY_PROMPT, L"Prompt");

    switch (m_manager->GetDropPolicy()) {
    case DropPolicy::Move:
        CheckMenuItem(policyMenu, ID_DROP_POLICY_MOVE, MF_CHECKED);
        break;
    case DropPolicy::Copy:
        CheckMenuItem(policyMenu, ID_DROP_POLICY_COPY, MF_CHECKED);
        break;
    case DropPolicy::Prompt:
        CheckMenuItem(policyMenu, ID_DROP_POLICY_PROMPT, MF_CHECKED);
        break;
    }

    AppendMenuW(settingsMenu, MF_STRING, ID_TRAY_SHOW_INFO_NOTIFICATIONS, L"Show Informational Notifications");
    CheckMenuItem(settingsMenu, ID_TRAY_SHOW_INFO_NOTIFICATIONS,
                  MF_BYCOMMAND | (m_showInfoNotifications ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(settingsMenu, MF_POPUP, (UINT_PTR)policyMenu, L"Drop Policy");

    AppendMenuW(menu, MF_POPUP, (UINT_PTR)settingsMenu, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);

    UINT cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        pt.x, pt.y, 0,
        m_hwnd, nullptr);

    DestroyMenu(menu);

    if (cmd == ID_TRAY_NEW_SPACE) {
        m_manager->CreateSpaceAt(pt);
    } else if (cmd == ID_TRAY_NEW_PORTAL_SPACE) {
        const std::wstring folder = PickFolderPath(m_hwnd);
        if (!folder.empty()) {
            m_manager->CreatePortalSpaceAt(pt, folder);
        }
    } else if (cmd == ID_TRAY_OPEN_FOLDER) {
        m_manager->OpenFirstSpaceBackingFolder();
    } else if (cmd == ID_TRAY_SHOW_INFO_NOTIFICATIONS) {
        SetShowInfoNotifications(!m_showInfoNotifications);
    } else if (cmd == ID_DROP_POLICY_MOVE) {
        m_manager->SetDropPolicy(DropPolicy::Move);
    } else if (cmd == ID_DROP_POLICY_COPY) {
        m_manager->SetDropPolicy(DropPolicy::Copy);
    } else if (cmd == ID_DROP_POLICY_PROMPT) {
        m_manager->SetDropPolicy(DropPolicy::Prompt);
    } else if (cmd == ID_TRAY_EXIT) {
        m_manager->ExitApplication();
    }
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TrayIcon* self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!self && msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TrayIcon*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }

    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    if (msg == WMAPP_TRAY && lParam == WM_RBUTTONUP) {
        self->ShowMenu();
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
