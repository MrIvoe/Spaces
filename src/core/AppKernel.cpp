#include "core/AppKernel.h"

#include "App.h"
#include "SpaceManager.h"
#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "core/EventBus.h"
#include "core/PluginCatalogFetcher.h"
#include "core/ServiceRegistry.h"
#include "core/SettingsStore.h"
#include "core/ThemeMigrationService.h"
#include "core/ThemePlatform.h"
#include "extensions/SpaceExtensionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginHost.h"
#include "extensions/PluginRegistry.h"
#include "extensions/PluginSettingsRegistry.h"
#include "Win32Helpers.h"

#include <array>
#include <cstring>
#include <cwctype>
#include <exception>

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

    std::wstring BuildUtcTimestampIso8601()
    {
        SYSTEMTIME utc{};
        GetSystemTime(&utc);

        wchar_t buffer[32] = {};
        swprintf_s(
            buffer,
            L"%04u-%02u-%02uT%02u:%02u:%02uZ",
            static_cast<unsigned>(utc.wYear),
            static_cast<unsigned>(utc.wMonth),
            static_cast<unsigned>(utc.wDay),
            static_cast<unsigned>(utc.wHour),
            static_cast<unsigned>(utc.wMinute),
            static_cast<unsigned>(utc.wSecond));
        return buffer;
    }

    int ParseIntClamped(const std::wstring& value, int fallback, int minValue, int maxValue)
    {
        int parsed = fallback;
        try
        {
            parsed = std::stoi(value);
        }
        catch (...)
        {
            parsed = fallback;
        }

        if (parsed < minValue)
        {
            parsed = minValue;
        }
        if (parsed > maxValue)
        {
            parsed = maxValue;
        }

        return parsed;
    }

    namespace Win32ThemeSystem
    {
        std::wstring NormalizeThemeId(std::wstring themeId)
        {
            for (auto& ch : themeId)
            {
                if (ch == L'_')
                {
                    ch = L'-';
                }
                else
                {
                    ch = static_cast<wchar_t>(towlower(ch));
                }
            }

            if (themeId.empty())
            {
                return L"graphite-office";
            }

            return themeId;
        }

        std::wstring DisplayNameFromThemeId(const std::wstring& themeId)
        {
            static const std::array<std::pair<const wchar_t*, const wchar_t*>, 20> kCatalog = {{
                {L"amber-terminal", L"Amber Terminal"},
                {L"arctic-glass", L"Arctic Glass"},
                {L"aurora-light", L"Aurora Light"},
                {L"brass-steampunk", L"Brass Steampunk"},
                {L"copper-foundry", L"Copper Foundry"},
                {L"emerald-ledger", L"Emerald Ledger"},
                {L"forest-organic", L"Forest Organic"},
                {L"graphite-office", L"Graphite Office"},
                {L"harbor-blue", L"Harbor Blue"},
                {L"ivory-bureau", L"Ivory Bureau"},
                {L"mono-minimal", L"Mono Minimal"},
                {L"neon-cyberpunk", L"Neon Cyberpunk"},
                {L"nocturne-dark", L"Nocturne Dark"},
                {L"nova-futuristic", L"Nova Futuristic"},
                {L"olive-terminal", L"Olive Terminal"},
                {L"pop-colorburst", L"Pop Colorburst"},
                {L"rose-paper", L"Rose Paper"},
                {L"storm-steel", L"Storm Steel"},
                {L"sunset-retro", L"Sunset Retro"},
                {L"tape-lo-fi", L"Tape Lo-Fi"},
            }};

            for (const auto& entry : kCatalog)
            {
                if (themeId == entry.first)
                {
                    return entry.second;
                }
            }

            return L"Graphite Office";
        }

        void Apply(SettingsStore* store, const std::wstring& themeId)
        {
            if (!store)
            {
                return;
            }

            const std::wstring normalizedThemeId = NormalizeThemeId(themeId);
            const std::wstring displayName = DisplayNameFromThemeId(normalizedThemeId);

            if (store->Get(L"theme.source", L"") != L"win32_theme_system")
            {
                store->Set(L"theme.source", L"win32_theme_system");
            }
            if (store->Get(L"theme.win32.theme_id", L"") != normalizedThemeId)
            {
                store->Set(L"theme.win32.theme_id", normalizedThemeId);
            }
            if (store->Get(L"theme.win32.display_name", L"") != displayName)
            {
                store->Set(L"theme.win32.display_name", displayName);
            }
            if (store->Get(L"theme.win32.catalog_version", L"") != L"2026.04.06")
            {
                store->Set(L"theme.win32.catalog_version", L"2026.04.06");
            }
        }
    }

    SpaceMetadata ToSpaceMetadata(const SpaceModel& space)
    {
        SpaceMetadata meta;
        meta.id = space.id;
        meta.title = space.title;
        meta.backingFolderPath = space.backingFolder;
        meta.contentType = space.contentType;
        meta.contentPluginId = space.contentPluginId;
        meta.contentSource = space.contentSource;
        meta.contentState = space.contentState;
        meta.contentStateDetail = space.contentStateDetail;
        return meta;
    }
}

class AppKernel::KernelAppCommands final : public IApplicationCommands
{
public:
    KernelAppCommands(App* app, AppKernel* kernel) : m_app(app), m_kernel(kernel)
    {
    }

    std::wstring CreateSpaceNearCursor() override
    {
        SpaceCreateRequest request;
        return CreateSpaceNearCursor(request);
    }

    std::wstring CreateSpaceNearCursor(const SpaceCreateRequest& request) override
    {
        if (m_app)
        {
            return m_app->CreateSpaceNearCursor(request);
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

    SpaceMetadata GetActiveSpaceMetadata() const override
    {
        SpaceMetadata meta;
        if (!m_app)
        {
            return meta;
        }

        SpaceManager* manager = m_app->GetSpaceManager();
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
        const SpaceModel* space = manager->FindSpaceByWindow(root ? root : hovered);
        if (!space)
        {
            return meta;
        }

        return ToSpaceMetadata(*space);
    }

    std::vector<std::wstring> GetAllSpaceIds() const override
    {
        if (!m_app || !m_app->GetSpaceManager())
        {
            return {};
        }

        return m_app->GetSpaceManager()->GetAllSpaceIds();
    }

    SpaceMetadata GetSpaceMetadata(const std::wstring& spaceId) const override
    {
        SpaceMetadata meta;
        if (!m_app)
        {
            return meta;
        }

        SpaceManager* manager = m_app->GetSpaceManager();
        if (!manager)
        {
            return meta;
        }

        const SpaceModel* space = manager->FindSpace(spaceId);
        if (!space)
        {
            return meta;
        }

        return ToSpaceMetadata(*space);
    }

    void RefreshSpace(const std::wstring& spaceId) override
    {
        if (!m_app)
        {
            return;
        }

        SpaceManager* manager = m_app->GetSpaceManager();
        if (!manager)
        {
            return;
        }

        manager->RefreshSpace(spaceId);
    }

    void UpdateSpaceContentSource(const std::wstring& spaceId, const std::wstring& contentSource) override
    {
        if (m_app && m_app->GetSpaceManager())
        {
            m_app->GetSpaceManager()->SetSpaceContentSource(spaceId, contentSource);
        }
    }

    void UpdateSpaceContentState(const std::wstring& spaceId,
                                 const std::wstring& state,
                                 const std::wstring& detail) override
    {
        if (m_app && m_app->GetSpaceManager())
        {
            m_app->GetSpaceManager()->SetSpaceContentState(spaceId, state, detail);
        }
    }

    void UpdateSpacePresentation(const std::wstring& spaceId,
                                 const SpacePresentationSettings& settings) override
    {
        if (m_app && m_app->GetSpaceManager())
        {
            m_app->GetSpaceManager()->ApplySpacePresentation(spaceId, settings);
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
    m_spaceExtensionRegistry = std::make_unique<SpaceExtensionRegistry>();
    m_pluginHost = std::make_unique<PluginHost>();
    m_appCommands = std::make_unique<KernelAppCommands>(app, this);

    // Create and load persistent settings store
    m_settingsStore = std::make_unique<SettingsStore>();
    const auto settingsPath = Win32Helpers::GetSpacesRoot() / L"settings.json";
    m_settingsStore->Load(settingsPath);
    if (m_diagnostics)
    {
        m_diagnostics->SetStore(m_settingsStore.get());
    }

    // Run idempotent theme migration before any theme rendering.
    ThemeMigrationService themeMigration(m_settingsStore.get());
    if (!themeMigration.Migrate())
    {
        if (m_diagnostics)
            m_diagnostics->Warn(L"Theme migration encountered an error; using defaults");
    }

    m_settingsRegistry->SetStore(m_settingsStore.get());
    m_themePlatform = std::make_unique<ThemePlatform>(m_settingsStore.get());

    m_settingsObserverToken = m_settingsRegistry->RegisterObserver(
        [this, app](const std::wstring& key, const std::wstring& value)
        {
            (void)value;

            if (!m_settingsRegistry)
            {
                return;
            }

            if (key == L"appearance.text.scale_percent")
            {
                const std::wstring legacy = m_settingsRegistry->GetValue(L"appearance.theme.text_scale_percent", L"115");
                if (legacy != value)
                {
                    m_settingsRegistry->SetValue(L"appearance.theme.text_scale_percent", value);
                }
            }
            else if (key == L"spaces.files.show_hidden")
            {
                const std::wstring legacy = m_settingsRegistry->GetValue(L"explorer.portal.show_hidden", L"false");
                if (legacy != value)
                {
                    m_settingsRegistry->SetValue(L"explorer.portal.show_hidden", value);
                }
            }
            else if (key == L"spaces.window.close_to_tray")
            {
                const std::wstring legacy = m_settingsRegistry->GetValue(L"tray.behavior.close_to_tray", L"true");
                if (legacy != value)
                {
                    m_settingsRegistry->SetValue(L"tray.behavior.close_to_tray", value);
                }
            }
            else if (key == L"spaces.create.auto_focus")
            {
                const std::wstring legacy = m_settingsRegistry->GetValue(L"core.behavior.auto_focus_on_create", L"true");
                if (legacy != value)
                {
                    m_settingsRegistry->SetValue(L"core.behavior.auto_focus_on_create", value);
                }
            }

            if (key == L"settings.plugins.manager_action" && value == L"apply_now")
            {
                if (!m_pluginHost)
                {
                    return;
                }

                PluginContext pluginContext;
                pluginContext.commandDispatcher = m_commandDispatcher.get();
                pluginContext.eventBus = m_eventBus.get();
                pluginContext.diagnostics = m_diagnostics.get();
                pluginContext.settingsRegistry = m_settingsRegistry.get();
                pluginContext.menuRegistry = m_menuRegistry.get();
                pluginContext.spaceExtensionRegistry = m_spaceExtensionRegistry.get();
                pluginContext.appCommands = m_appCommands.get();

                const bool reloaded = m_pluginHost->ReloadBuiltins(pluginContext);

                if (m_settingsRegistry)
                {
                    int loadedCount = 0;
                    int failedCount = 0;
                    int disabledCount = 0;

                    const auto& statuses = m_pluginHost->GetRegistry().GetAll();
                    for (const auto& status : statuses)
                    {
                        if (!status.enabled)
                        {
                            ++disabledCount;
                        }
                        else if (status.loaded)
                        {
                            ++loadedCount;
                        }
                        else
                        {
                            ++failedCount;
                        }
                    }

                    const std::wstring summary =
                        L"loaded=" + std::to_wstring(loadedCount) +
                        L", failed=" + std::to_wstring(failedCount) +
                        L", disabled=" + std::to_wstring(disabledCount);
                    m_settingsRegistry->SetValue(L"settings.plugins.last_reload_summary", summary);
                    m_settingsRegistry->SetValue(L"settings.plugins.last_reload_utc", BuildUtcTimestampIso8601());
                }

                if (m_diagnostics)
                {
                    if (reloaded)
                    {
                        m_diagnostics->Info(L"Plugin host reloaded after settings.plugins.manager_action=apply_now");
                    }
                    else
                    {
                        m_diagnostics->Warn(L"Plugin host reload after settings.plugins.manager_action=apply_now completed with one or more plugin failures");
                    }
                }

                return;
            }

            if (key != L"theme.win32.theme_id" &&
                key != L"theme.win32.display_name" &&
                key != L"theme.source" &&
                key != L"theme.preset" &&
                key != L"appearance.theme.mode" &&
                key != L"appearance.theme.text_scale_percent" &&
                key != L"appearance.text.scale_percent" &&
                key != L"appearance.ui.icon_size" &&
                key != L"appearance.ui.opacity_profile" &&
                key != L"appearance.ui.transparency_enabled" &&
                key != L"appearance.ui.settings_density" &&
                key != L"appearance.ui.toggle_size" &&
                key != L"appearance.ui.tray_menu_size" &&
                key != L"appearance.ui.space_titlebar_opacity_percent" &&
                key != L"appearance.ui.space_idle_opacity_percent" &&
                key != L"appearance.ui.settings_window_opacity_percent" &&
                key != L"appearance.ui.settings_window_blur_enabled" &&
                key != L"appearance.ui.settings_row_height_px" &&
                key != L"appearance.ui.settings_row_gap_px" &&
                key != L"appearance.ui.settings_section_gap_px" &&
                key != L"appearance.ui.settings_toggle_width_px" &&
                key != L"appearance.ui.settings_toggle_height_px" &&
                key != L"appearance.ui.tray_menu_min_width_px" &&
                key != L"appearance.ui.tray_menu_row_height_px" &&
                key != L"appearance.ui.component_family" &&
                key != L"appearance.ui.controls_family" &&
                key != L"appearance.ui.button_family" &&
                key != L"appearance.ui.menu_style" &&
                key != L"appearance.ui.fence_style" &&
                key != L"appearance.ui.motion_preset" &&
                key != L"appearance.ui.icon_pack" &&
                key != L"appearance.icons.pack")
            {
                return;
            }

            const std::wstring rawThemeId = m_settingsRegistry->GetValue(L"theme.win32.theme_id", L"");
            const std::wstring themeId = rawThemeId.empty()
                ? Win32ThemeSystem::NormalizeThemeId(m_settingsRegistry->GetValue(L"theme.preset", L"graphite-office"))
                : Win32ThemeSystem::NormalizeThemeId(rawThemeId);

            if (m_diagnostics)
            {
                m_diagnostics->Info(L"Theme bridge apply requested: id='" + themeId + L"'");
            }

            SendNotifyMessageW(HWND_BROADCAST, ThemePlatform::GetThemeChangedMessageId(), 0, 0);
        });

    const bool createRegistered = m_commandDispatcher->RegisterCommand(L"space.create", [commands = m_appCommands.get(), settings = m_settingsRegistry.get()]() {
        SpaceCreateRequest request;
        if (settings)
        {
            std::wstring title = settings->GetValue(L"spaces.create.title_template", L"");
            if (title.empty())
            {
                title = settings->GetValue(L"core.behavior.default_title", L"New Space");
            }
            request.title = Trim(title);

            const std::wstring sizePreset = settings->GetValue(L"spaces.create.size_preset", L"");
            if (sizePreset == L"small")
            {
                request.width = 280;
                request.height = 200;
            }
            else if (sizePreset == L"normal")
            {
                request.width = 320;
                request.height = 240;
            }
            else if (sizePreset == L"large")
            {
                request.width = 420;
                request.height = 300;
            }
            else if (sizePreset == L"xlarge")
            {
                request.width = 520;
                request.height = 360;
            }
            else
            {
                // Backward compatibility: honor legacy numeric width/height keys.
                request.width = ParseIntClamped(
                    settings->GetValue(L"spaces.create.default_width", L"320"),
                    320,
                    120,
                    2400);
                request.height = ParseIntClamped(
                    settings->GetValue(L"spaces.create.default_height", L"240"),
                    240,
                    60,
                    1800);
            }
        }

        commands->CreateSpaceNearCursor(request);
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
    context.spaceExtensionRegistry = m_spaceExtensionRegistry.get();
    context.appCommands = m_appCommands.get();

    const bool loaded = m_pluginHost ? m_pluginHost->LoadBuiltins(context) : false;
    if (m_diagnostics)
    {
        if (loaded)
        {
            m_diagnostics->Info(L"Builtin plugins loaded during AppKernel startup.");
        }
        else
        {
            m_diagnostics->Warn(L"One or more built-in plugins failed during AppKernel startup; continuing in degraded mode.");
        }
    }
    if (m_diagnostics)
    {
        m_diagnostics->Info(L"AppKernel initialized with plugin host");
    }

    // NOTE: Startup catalog health check is temporarily disabled because it can
    // trigger a release-only startup crash in some environments. Marketplace
    // checks still run from settings workflows.

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
    m_spaceExtensionRegistry.reset();
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
        items.push_back(TrayMenuEntry{L"New Space", L"space.create", L"actions.space.create", false});
        items.push_back(TrayMenuEntry{L"Exit", L"app.exit", L"actions.app.exit", true});
        return items;
    }

    const auto contributions = m_menuRegistry->GetBySurface(MenuSurface::Tray);
    items.reserve(contributions.size());

    for (const auto& entry : contributions)
    {
        TrayMenuEntry item;
        item.title = entry.title;
        item.commandId = entry.commandId;
        item.iconKey = L"actions." + entry.commandId;
        item.separatorBefore = entry.separatorBefore;
        items.push_back(std::move(item));
    }

    if (items.empty())
    {
        items.push_back(TrayMenuEntry{L"New Space", L"space.create", L"actions.space.create", false});
        items.push_back(TrayMenuEntry{L"Exit", L"app.exit", L"actions.app.exit", true});
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
        view.compatibilityStatus = status.compatibilityStatus;
        view.compatibilityReason = status.compatibilityReason;
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

const SpaceExtensionRegistry* AppKernel::GetSpaceExtensionRegistry() const
{
    return m_spaceExtensionRegistry.get();
}

PluginSettingsRegistry* AppKernel::GetSettingsRegistry() const
{
    return m_settingsRegistry.get();
}

const ThemePlatform* AppKernel::GetThemePlatform() const
{
    return m_themePlatform.get();
}
