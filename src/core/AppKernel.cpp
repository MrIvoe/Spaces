#include "core/AppKernel.h"

#include "App.h"
#include "FenceManager.h"
#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "core/EventBus.h"
#include "core/ServiceRegistry.h"
#include "core/SettingsStore.h"
#include "core/ThemePlatform.h"
#include "extensions/FenceExtensionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginHost.h"
#include "extensions/PluginRegistry.h"
#include "extensions/PluginSettingsRegistry.h"
#include "Win32Helpers.h"

#include <cwctype>

#include <windows.h>

namespace
{
    std::wstring Trim(const std::wstring& text)
    {
        size_t start = 0;
        while (start < text.size() && iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();
        while (end > start && iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(start, end - start);
    }

    namespace Win32ThemeSystem
    {
        void Apply(SettingsStore* store, const std::wstring& displayName)
        {
            if (!store)
            {
                return;
            }

            std::wstring normalized = Trim(displayName);
            if (normalized.empty())
            {
                normalized = L"Graphite Office";
            }

            if (store->Get(L"theme.source", L"") != L"win32_theme_system")
            {
                store->Set(L"theme.source", L"win32_theme_system");
            }
            if (store->Get(L"theme.win32.display_name", L"") != normalized)
            {
                store->Set(L"theme.win32.display_name", normalized);
            }
        }
    }

    FenceMetadata ToFenceMetadata(const FenceModel& fence)
    {
        FenceMetadata meta;
        meta.id = fence.id;
        meta.title = fence.title;
        meta.backingFolderPath = fence.backingFolder;
        meta.contentType = fence.contentType;
        meta.contentPluginId = fence.contentPluginId;
        meta.contentSource = fence.contentSource;
        meta.contentState = fence.contentState;
        meta.contentStateDetail = fence.contentStateDetail;
        return meta;
    }
}

class AppKernel::KernelAppCommands final : public IApplicationCommands
{
public:
    KernelAppCommands(App* app, AppKernel* kernel) : m_app(app), m_kernel(kernel)
    {
    }

    std::wstring CreateFenceNearCursor() override
    {
        FenceCreateRequest request;
        return CreateFenceNearCursor(request);
    }

    std::wstring CreateFenceNearCursor(const FenceCreateRequest& request) override
    {
        if (m_app)
        {
            return m_app->CreateFenceNearCursor(request);
        }

        return L"";
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

    CommandContext GetCurrentCommandContext() const override
    {
        if (!m_kernel)
        {
            return {};
        }

        std::lock_guard<std::mutex> lock(m_kernel->m_commandContextMutex);
        return m_kernel->m_currentCommandContext;
    }

    FenceMetadata GetActiveFenceMetadata() const override
    {
        FenceMetadata meta;
        if (!m_app)
        {
            return meta;
        }

        FenceManager* manager = m_app->GetFenceManager();
        if (!manager)
        {
            return meta;
        }

        POINT cursor{};
        if (!GetCursorPos(&cursor))
        {
            return meta;
        }

        HWND hovered = WindowFromPoint(cursor);
        if (!hovered)
        {
            return meta;
        }

        HWND root = GetAncestor(hovered, GA_ROOT);
        const FenceModel* fence = manager->FindFenceByWindow(root ? root : hovered);
        if (!fence)
        {
            return meta;
        }

        return ToFenceMetadata(*fence);
    }

    std::vector<std::wstring> GetAllFenceIds() const override
    {
        if (!m_app || !m_app->GetFenceManager())
        {
            return {};
        }

        return m_app->GetFenceManager()->GetAllFenceIds();
    }

    FenceMetadata GetFenceMetadata(const std::wstring& fenceId) const override
    {
        FenceMetadata meta;
        if (!m_app)
        {
            return meta;
        }

        FenceManager* manager = m_app->GetFenceManager();
        if (!manager)
        {
            return meta;
        }

        const FenceModel* fence = manager->FindFence(fenceId);
        if (!fence)
        {
            return meta;
        }

        return ToFenceMetadata(*fence);
    }

    void RefreshFence(const std::wstring& fenceId) override
    {
        if (!m_app)
        {
            return;
        }

        FenceManager* manager = m_app->GetFenceManager();
        if (!manager)
        {
            return;
        }

        manager->RefreshFence(fenceId);
    }

    void UpdateFenceContentSource(const std::wstring& fenceId, const std::wstring& contentSource) override
    {
        if (m_app && m_app->GetFenceManager())
        {
            m_app->GetFenceManager()->SetFenceContentSource(fenceId, contentSource);
        }
    }

    void UpdateFenceContentState(const std::wstring& fenceId,
                                 const std::wstring& state,
                                 const std::wstring& detail) override
    {
        if (m_app && m_app->GetFenceManager())
        {
            m_app->GetFenceManager()->SetFenceContentState(fenceId, state, detail);
        }
    }

    void UpdateFencePresentation(const std::wstring& fenceId,
                                 const FencePresentationSettings& settings) override
    {
        if (m_app && m_app->GetFenceManager())
        {
            m_app->GetFenceManager()->ApplyFencePresentation(fenceId, settings);
        }
    }

private:
    App* m_app = nullptr;
    AppKernel* m_kernel = nullptr;
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
    m_appCommands = std::make_unique<KernelAppCommands>(app, this);

    // Create and load persistent settings store
    m_settingsStore = std::make_unique<SettingsStore>();
    const auto settingsPath = Win32Helpers::GetFencesRoot() / L"settings.json";
    m_settingsStore->Load(settingsPath);
    m_settingsRegistry->SetStore(m_settingsStore.get());
    m_themePlatform = std::make_unique<ThemePlatform>(m_settingsStore.get());

    m_settingsObserverToken = m_settingsRegistry->RegisterObserver(
        [this, app](const std::wstring& key, const std::wstring& value)
        {
            if (!m_settingsRegistry)
            {
                return;
            }

            if (key != L"theme.win32.display_name" && key != L"theme.source")
            {
                return;
            }

            const std::wstring source = m_settingsRegistry->GetValue(L"theme.source", L"");
            if (source != L"win32_theme_system")
            {
                return;
            }

            std::wstring displayName = m_settingsRegistry->GetValue(L"theme.win32.display_name", L"Graphite Office");
            if (displayName.empty())
            {
                displayName = L"Graphite Office";
                m_settingsRegistry->SetValue(L"theme.win32.display_name", displayName);
                return;
            }

            Win32ThemeSystem::Apply(m_settingsStore.get(), displayName);

            if (m_diagnostics)
            {
                m_diagnostics->Info(L"Theme bridge apply requested: " + displayName);
            }

            if (app && app->GetFenceManager())
            {
                app->GetFenceManager()->RefreshAll();
            }

            SendNotifyMessageW(HWND_BROADCAST, ThemePlatform::GetThemeChangedMessageId(), 0, 0);
            SendNotifyMessageW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0);
        });

    const bool createRegistered = m_commandDispatcher->RegisterCommand(L"fence.create", [commands = m_appCommands.get()]() {
        commands->CreateFenceNearCursor();
    });
    const bool exitRegistered = m_commandDispatcher->RegisterCommand(L"app.exit", [commands = m_appCommands.get()]() {
        commands->ExitApplication();
    });
    const bool settingsRegistered = m_commandDispatcher->RegisterCommand(L"plugin.openSettings", [commands = m_appCommands.get()]() {
        commands->OpenSettings();
    });

    if (m_diagnostics && (!createRegistered || !exitRegistered || !settingsRegistered))
    {
        m_diagnostics->Warn(L"One or more core commands were not registered. Command collisions may exist.");
    }

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
    if (m_settingsRegistry && m_settingsObserverToken != 0)
    {
        m_settingsRegistry->UnregisterObserver(m_settingsObserverToken);
        m_settingsObserverToken = 0;
    }

    if (m_pluginHost)
    {
        m_pluginHost->Shutdown();
    }

    m_pluginHost.reset();
    m_fenceExtensionRegistry.reset();
    m_themePlatform.reset();
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
    CommandContext context;
    context.commandId = commandId;
    return ExecuteCommand(commandId, context);
}

bool AppKernel::ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const
{
    if (!m_commandDispatcher)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_commandContextMutex);
        m_currentCommandContext = context;
        if (m_currentCommandContext.commandId.empty())
        {
            m_currentCommandContext.commandId = commandId;
        }
    }

    const auto result = m_commandDispatcher->DispatchDetailed(commandId, context);

    {
        std::lock_guard<std::mutex> lock(m_commandContextMutex);
        m_currentCommandContext = {};
    }

    if (!result.handled)
    {
        if (m_diagnostics)
        {
            m_diagnostics->Warn(L"Unknown command requested: " + commandId);
        }
        return false;
    }

    if (!result.succeeded)
    {
        if (m_diagnostics)
        {
            m_diagnostics->Error(L"Command failed: id='" + commandId + L"' error='" + result.error + L"'");
        }
        return false;
    }

    return true;
}

const MenuContributionRegistry* AppKernel::GetMenuRegistry() const
{
    return m_menuRegistry.get();
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

const ThemePlatform* AppKernel::GetThemePlatform() const
{
    return m_themePlatform.get();
}
