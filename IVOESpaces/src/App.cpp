#include "App.h"
#include "DesktopItemService.h"
#include "DesktopWatcher.h"
#include "ShellDesktop.h"
#include "SpaceManager.h"
#include "TrayIcon.h"
#include "ZOrderCoordinator.h"
#include "ShellChangeWatcher.h"
#include <commctrl.h>

App::App(HINSTANCE instance) : m_instance(instance) {}

App::~App() = default;

bool App::InitCommonControls() {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    return true;
}

bool App::InitDesktopHost() {
    const ShellDesktop::HostInfo hostInfo = ShellDesktop::FindDesktopHostInfo();
    m_desktopHost = hostInfo.host;
    m_hostMode = hostInfo.mode;
    if (!m_desktopHost) {
        m_desktopHost = GetDesktopWindow();
    }
    return m_desktopHost != nullptr;
}

bool App::InitManagers() {
    m_desktopItemService = std::make_unique<DesktopItemService>();
    if (!m_desktopItemService->Initialize()) {
        m_desktopItemService.reset();
    } else {
        m_desktopWatcher = std::make_unique<DesktopWatcher>(
            m_desktopItemService->GetDesktopPath(),
            [this]() {
                PostThreadMessageW(m_uiThreadId, WM_APP_DESKTOP_CHANGED, 0, 0);
            });
        if (!m_desktopWatcher->Initialize()) {
            m_desktopWatcher.reset();
        }
    }

    m_spaceManager = std::make_unique<SpaceManager>(m_instance, m_desktopHost);
    if (!m_spaceManager->Initialize()) {
        return false;
    }

    m_trayIcon = std::make_unique<TrayIcon>(m_instance, m_spaceManager.get());
    if (!m_trayIcon->Initialize()) {
        m_trayIcon.reset();
    } else {
        m_spaceManager->SetStatusCallback([this](const std::wstring& title, const std::wstring& message, bool error) {
            if (m_trayIcon) {
                m_trayIcon->ShowNotification(title, message, error);
            }
        });
    }

    m_zOrderCoordinator = std::make_unique<ZOrderCoordinator>(m_instance, m_spaceManager.get());
    if (!m_zOrderCoordinator->Initialize()) {
        m_zOrderCoordinator.reset();
    }

    m_shellChangeWatcher = std::make_unique<ShellChangeWatcher>(m_instance, m_spaceManager.get());
    if (!m_shellChangeWatcher->Initialize()) {
        m_shellChangeWatcher.reset();
    }

    m_spaceManager->RestoreOrCreateDefaultSpace();
    m_spaceManager->MaintainDesktopPlacement();
    return true;
}

bool App::Initialize() {
    m_uiThreadId = GetCurrentThreadId();
    if (!InitCommonControls()) return false;
    if (!InitDesktopHost()) return false;
    if (!InitManagers()) return false;
    return true;
}

int App::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_APP_DESKTOP_CHANGED) {
            if (m_spaceManager) {
                m_spaceManager->OnShellChanged();
            }
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
