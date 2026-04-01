#pragma once

#include <memory>
#include <vector>

#include "core/KernelViews.h"
#include "extensions/MenuContributionRegistry.h"

class App;
class CommandDispatcher;
class EventBus;
class Diagnostics;
class ServiceRegistry;
class PluginHost;
class PluginSettingsRegistry;
class FenceExtensionRegistry;

struct TrayMenuEntry
{
    std::wstring title;
    std::wstring commandId;
    bool separatorBefore = false;
};

class AppKernel
{
public:
    AppKernel();
    ~AppKernel();

    bool Initialize(App* app);
    void Shutdown();

    bool ExecuteCommand(const std::wstring& commandId) const;
    std::vector<TrayMenuEntry> GetTrayMenuEntries() const;
    std::vector<PluginStatusView> GetPluginStatuses() const;
    std::vector<SettingsPageView> GetSettingsPages() const;
    const FenceExtensionRegistry* GetFenceExtensionRegistry() const;

private:
    class KernelAppCommands;

    std::unique_ptr<CommandDispatcher> m_commandDispatcher;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Diagnostics> m_diagnostics;
    std::unique_ptr<ServiceRegistry> m_serviceRegistry;
    std::unique_ptr<MenuContributionRegistry> m_menuRegistry;
    std::unique_ptr<PluginSettingsRegistry> m_settingsRegistry;
    std::unique_ptr<FenceExtensionRegistry> m_fenceExtensionRegistry;
    std::unique_ptr<PluginHost> m_pluginHost;
    std::unique_ptr<KernelAppCommands> m_appCommands;
};
