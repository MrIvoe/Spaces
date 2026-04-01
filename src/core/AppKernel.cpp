#include "core/AppKernel.h"

#include "App.h"
#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "core/EventBus.h"
#include "core/ServiceRegistry.h"
#include "core/SettingsStore.h"
#include "extensions/FenceExtensionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginHost.h"
#include "extensions/PluginRegistry.h"
#include "extensions/PluginSettingsRegistry.h"
#include "Win32Helpers.h"

class AppKernel::KernelAppCommands final : public IApplicationCommands
{
public:
    explicit KernelAppCommands(App* app) : m_app(app)
    {
    }

    void CreateFenceNearCursor() override
    {
        if (m_app)
        {
            m_app->CreateFenceNearCursor();
        }
    }

    void ExitApplication() override
    {
        if (m_app)
        {
            m_app->Exit();
        }
    }

    void OpenSettings() override
    {
        if (m_app)
        {
            m_app->OpenSettingsWindow();
        }
    }

private:
    App* m_app = nullptr;
};

AppKernel::AppKernel() = default;

AppKernel::~AppKernel()
{
    Shutdown();
}

bool AppKernel::Initialize(App* app)
{
    m_commandDispatcher = std::make_unique<CommandDispatcher>();
    m_eventBus = std::make_unique<EventBus>();
    m_diagnostics = std::make_unique<Diagnostics>();
    m_serviceRegistry = std::make_unique<ServiceRegistry>();
    m_menuRegistry = std::make_unique<MenuContributionRegistry>();
    m_settingsRegistry = std::make_unique<PluginSettingsRegistry>();
    m_fenceExtensionRegistry = std::make_unique<FenceExtensionRegistry>();
    m_pluginHost = std::make_unique<PluginHost>();
    m_appCommands = std::make_unique<KernelAppCommands>(app);

    // Create and load persistent settings store
    m_settingsStore = std::make_unique<SettingsStore>();
    const auto settingsPath = Win32Helpers::GetFencesRoot() / L"settings.json";
    m_settingsStore->Load(settingsPath);
    m_settingsRegistry->SetStore(m_settingsStore.get());

    m_commandDispatcher->RegisterCommand(L"fence.create", [commands = m_appCommands.get()]() {
        commands->CreateFenceNearCursor();
    });
    m_commandDispatcher->RegisterCommand(L"app.exit", [commands = m_appCommands.get()]() {
        commands->ExitApplication();
    });
    m_commandDispatcher->RegisterCommand(L"plugin.openSettings", [commands = m_appCommands.get()]() {
        commands->OpenSettings();
    });

    PluginContext context;
    context.commandDispatcher = m_commandDispatcher.get();
    context.eventBus = m_eventBus.get();
    context.diagnostics = m_diagnostics.get();
    context.settingsRegistry = m_settingsRegistry.get();
    context.menuRegistry = m_menuRegistry.get();
    context.fenceExtensionRegistry = m_fenceExtensionRegistry.get();
    context.appCommands = m_appCommands.get();

    const bool loaded = m_pluginHost->LoadBuiltins(context);
    if (m_diagnostics)
    {
        m_diagnostics->Info(L"AppKernel initialized with plugin host");
    }

    return loaded;
}

void AppKernel::Shutdown()
{
    if (m_pluginHost)
    {
        m_pluginHost->Shutdown();
    }

    m_pluginHost.reset();
    m_fenceExtensionRegistry.reset();
    m_settingsStore.reset();
    m_settingsRegistry.reset();
    m_menuRegistry.reset();
    m_serviceRegistry.reset();
    m_diagnostics.reset();
    m_eventBus.reset();
    m_commandDispatcher.reset();
    m_appCommands.reset();
}

bool AppKernel::ExecuteCommand(const std::wstring& commandId) const
{
    if (!m_commandDispatcher)
    {
        return false;
    }

    return m_commandDispatcher->Dispatch(commandId);
}

std::vector<TrayMenuEntry> AppKernel::GetTrayMenuEntries() const
{
    std::vector<TrayMenuEntry> items;
    if (!m_menuRegistry)
    {
        items.push_back(TrayMenuEntry{L"New Fence", L"fence.create", false});
        items.push_back(TrayMenuEntry{L"Exit", L"app.exit", true});
        return items;
    }

    const auto contributions = m_menuRegistry->GetBySurface(MenuSurface::Tray);
    items.reserve(contributions.size());

    for (const auto& entry : contributions)
    {
        TrayMenuEntry item;
        item.title = entry.title;
        item.commandId = entry.commandId;
        item.separatorBefore = entry.separatorBefore;
        items.push_back(std::move(item));
    }

    if (items.empty())
    {
        items.push_back(TrayMenuEntry{L"New Fence", L"fence.create", false});
        items.push_back(TrayMenuEntry{L"Exit", L"app.exit", true});
    }

    return items;
}

std::vector<PluginStatusView> AppKernel::GetPluginStatuses() const
{
    std::vector<PluginStatusView> views;
    if (!m_pluginHost)
    {
        return views;
    }

    const auto& statuses = m_pluginHost->GetRegistry().GetAll();
    views.reserve(statuses.size());
    for (const auto& status : statuses)
    {
        PluginStatusView view;
        view.id = status.manifest.id;
        view.displayName = status.manifest.displayName;
        view.version = status.manifest.version;
        view.enabled = status.enabled;
        view.loaded = status.loaded;
        view.lastError = status.lastError;
        view.capabilities = status.manifest.capabilities;
        views.push_back(std::move(view));
    }

    return views;
}

std::vector<SettingsPageView> AppKernel::GetSettingsPages() const
{
    std::vector<SettingsPageView> views;
    if (!m_settingsRegistry)
    {
        return views;
    }

    const auto pages = m_settingsRegistry->GetAllPages();
    views.reserve(pages.size());
    for (const auto& page : pages)
    {
        SettingsPageView view;
        view.pluginId = page.pluginId;
        view.pageId   = page.pageId;
        view.title    = page.title;
        view.order    = page.order;
        view.fields   = page.fields; // copy schema declared by the plugin
        views.push_back(std::move(view));
    }

    return views;
}

const FenceExtensionRegistry* AppKernel::GetFenceExtensionRegistry() const
{
    return m_fenceExtensionRegistry.get();
}

PluginSettingsRegistry* AppKernel::GetSettingsRegistry() const
{
    return m_settingsRegistry.get();
}
