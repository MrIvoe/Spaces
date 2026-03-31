#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>

class PluginManager;
class FenceManager;
class ShellIntegration;
class DesktopItemBroker;
class DragDropController;
class CommandRegistry;
class SettingsStore;
class Logger;

/// Main application host - manages all core subsystems
class AppHost {
public:
    AppHost();
    ~AppHost();

    /// Initialize the application
    bool Initialize();

    /// Run the application event loop
    int Run();

    /// Shutdown the application
    void Shutdown();

    /// Get subsystem references (for subsystems to access each other)
    PluginManager* GetPluginManager() const { return m_pluginManager.get(); }
    FenceManager* GetFenceManager() const { return m_fenceManager.get(); }
    ShellIntegration* GetShellIntegration() const { return m_shellIntegration.get(); }
    DesktopItemBroker* GetDesktopItemBroker() const { return m_desktopItemBroker.get(); }
    CommandRegistry* GetCommandRegistry() const { return m_commandRegistry.get(); }
    SettingsStore* GetSettingsStore() const { return m_settingsStore.get(); }
    Logger* GetLogger() const { return m_logger.get(); }

private:
    bool OnSingleInstanceCheck();
    bool InitializeComOle();
    bool LoadConfiguration();
    bool RestoreFences();
    bool StartTrayIcon();
    bool HookShellDesktop();

    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<SettingsStore> m_settingsStore;
    std::unique_ptr<ShellIntegration> m_shellIntegration;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<FenceManager> m_fenceManager;
    std::unique_ptr<DesktopItemBroker> m_desktopItemBroker;
    std::unique_ptr<DragDropController> m_dragDropController;
    std::unique_ptr<CommandRegistry> m_commandRegistry;

    HWND m_trayIconHwnd;
    bool m_isRunning;
    HANDLE m_instanceMutex;
};
