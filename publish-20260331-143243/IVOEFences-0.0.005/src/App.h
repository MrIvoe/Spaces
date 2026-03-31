#pragma once
#include <windows.h>
#include <memory>
#include "ShellDesktop.h"

class FenceManager;
class TrayIcon;
class ZOrderCoordinator;
class ShellChangeWatcher;
class DesktopItemService;
class DesktopWatcher;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    bool Initialize();
    int Run();

    HINSTANCE GetInstance() const { return m_instance; }
    HWND GetDesktopHost() const { return m_desktopHost; }

private:
    bool InitCommonControls();
    bool InitDesktopHost();
    bool InitManagers();

private:
    HINSTANCE m_instance{};
    HWND m_desktopHost{};
    ShellDesktop::HostMode m_hostMode{};
    std::unique_ptr<FenceManager> m_fenceManager;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::unique_ptr<ZOrderCoordinator> m_zOrderCoordinator;
    std::unique_ptr<ShellChangeWatcher> m_shellChangeWatcher;
    std::unique_ptr<DesktopItemService> m_desktopItemService;
    std::unique_ptr<DesktopWatcher> m_desktopWatcher;
};
