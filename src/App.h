#pragma once
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

#include "core/AppKernel.h"
#include "extensions/PluginContracts.h"

class SpaceStorage;
class Persistence;
class SpaceManager;
class TrayMenu;
class SettingsWindow;
class ThemePlatform;

class App
{
public:
    App();
    ~App();

    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Exit();

    std::wstring CreateSpaceNearCursor();
    std::wstring CreateSpaceNearCursor(const SpaceCreateRequest& request);
    SpaceManager* GetSpaceManager() const;
    bool ExecuteCommand(const std::wstring& commandId) const;
    bool ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const;
    std::vector<TrayMenuEntry> GetTrayMenuEntries() const;
    std::vector<PluginStatusView> GetPluginStatuses() const;
    std::vector<SettingsPageView> GetSettingsPages() const;
    void OpenSettingsWindow();
    const ThemePlatform* GetThemePlatform() const;

    HINSTANCE GetInstance() const { return m_hInstance; }

private:
    void Shutdown();

private:
    HINSTANCE m_hInstance = nullptr;
    bool m_shutdownStarted = false;
    HANDLE m_singleInstanceMutex = nullptr;
    std::unique_ptr<SpaceStorage> m_storage;
    std::unique_ptr<Persistence> m_persistence;
    std::unique_ptr<SpaceManager> m_manager;
    std::unique_ptr<TrayMenu> m_tray;
    std::unique_ptr<AppKernel> m_kernel;
    std::unique_ptr<SettingsWindow> m_settingsWindow;
};
