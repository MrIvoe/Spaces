#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "core/SettingsStore.h"
#include "core/ThemePlatform.h"
#include "Persistence.h"
#include "plugins/builtins/BuiltinPlugins.h"
#include "extensions/PluginContracts.h"
#include "extensions/FenceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginRegistry.h"
#include "extensions/PluginSettingsRegistry.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << "\n";
        return 1;
    }

    int TestCommandDispatcher()
    {
        CommandDispatcher dispatcher;
        int hitCount = 0;
        CommandContext receivedContext;

        if (!dispatcher.RegisterCommand(L"host.command", [&hitCount, &receivedContext](const CommandContext& context) {
            ++hitCount;
            receivedContext = context;
        }))
        {
            return Fail("register command should succeed");
        }

        if (dispatcher.RegisterCommand(L"host.command", []() {}, false))
        {
            return Fail("duplicate register without replace should fail");
        }

        if (!dispatcher.RegisterCommand(L"host.command", [&hitCount]() { hitCount += 10; }, true))
        {
            return Fail("duplicate register with replace should succeed");
        }

        const auto missing = dispatcher.DispatchDetailed(L"missing");
        if (missing.handled || missing.succeeded)
        {
            return Fail("missing command should be unhandled");
        }

        CommandContext context;
        context.commandId = L"host.command";
        context.invocationSource = L"test";
        context.fence.id = L"fence.context";
        if (!dispatcher.Dispatch(L"host.command", context) || hitCount != 10)
        {
            return Fail("replaced command should dispatch");
        }

        if (!dispatcher.RegisterCommand(L"host.context", [&receivedContext](const CommandContext& commandContext) {
            receivedContext = commandContext;
        }))
        {
            return Fail("context-aware command should register");
        }

        if (!dispatcher.Dispatch(L"host.context", context))
        {
            return Fail("context-aware command should dispatch");
        }

        if (receivedContext.fence.id != L"fence.context" || receivedContext.invocationSource != L"test")
        {
            return Fail("command context should flow through dispatch");
        }

        if (!dispatcher.RegisterCommand(L"host.crash", []() { throw std::runtime_error("boom"); }))
        {
            return Fail("register throwing command should succeed");
        }

        const auto crash = dispatcher.DispatchDetailed(L"host.crash");
        if (!crash.handled || crash.succeeded || crash.error.empty())
        {
            return Fail("throwing command should return handled failure details");
        }

        return 0;
    }

    int TestMenuRegistry()
    {
        MenuContributionRegistry registry;
        if (registry.Register(MenuContribution{MenuSurface::Tray, L"", L"cmd.empty", 10, false}))
        {
            return Fail("menu contribution with empty title should be rejected");
        }

        if (!registry.Register(MenuContribution{MenuSurface::Tray, L"B", L"cmd.b", 20, false}))
        {
            return Fail("valid menu contribution should register");
        }

        if (!registry.Register(MenuContribution{MenuSurface::Tray, L"A", L"cmd.a", 20, false}))
        {
            return Fail("second valid menu contribution should register");
        }

        if (registry.Register(MenuContribution{MenuSurface::Tray, L"A", L"cmd.a", 20, false}))
        {
            return Fail("duplicate contribution should be rejected");
        }

        const auto trayItems = registry.GetBySurface(MenuSurface::Tray);
        if (trayItems.size() != 2)
        {
            return Fail("tray menu should contain two unique items");
        }

        if (trayItems[0].title != L"A" || trayItems[1].title != L"B")
        {
            return Fail("menu items should be sorted by order then title");
        }

        registry.Clear();
        if (!registry.GetBySurface(MenuSurface::Tray).empty())
        {
            return Fail("clear should remove menu contributions");
        }

        return 0;
    }

    int TestSettingsRegistry()
    {
        PluginSettingsRegistry registry;

        PluginSettingsPage invalidPage;
        invalidPage.pluginId = L"builtin.test";
        invalidPage.pageId = L"invalid";
        invalidPage.title = L"Invalid";
        invalidPage.fields.push_back(SettingsFieldDescriptor{L"", L"No Key", L"", SettingsFieldType::String, L"", {}, 0});

        if (!registry.RegisterPage(invalidPage))
        {
            return Fail("page registration should succeed even if invalid fields are removed");
        }

        const auto invalidPages = registry.GetAllPages();
        if (invalidPages.size() != 1 || !invalidPages[0].fields.empty())
        {
            return Fail("invalid fields should be stripped from page");
        }

        PluginSettingsPage page;
        page.pluginId = L"builtin.test";
        page.pageId = L"valid";
        page.title = L"Valid";
        page.order = 10;
        page.fields.push_back(SettingsFieldDescriptor{
            L"appearance.mode",
            L"Mode",
            L"",
            SettingsFieldType::Enum,
            L"missing",
            {SettingsEnumOption{L"light", L"Light"}, SettingsEnumOption{L"dark", L"Dark"}},
            20});
        page.fields.push_back(SettingsFieldDescriptor{
            L"appearance.scale",
            L"Scale",
            L"",
            SettingsFieldType::Int,
            L"100",
            {},
            10});

        if (!registry.RegisterPage(page))
        {
            return Fail("valid page should register");
        }

        const auto pages = registry.GetAllPages();
        if (pages.size() != 2)
        {
            return Fail("expected two pages after valid registration");
        }

        const auto& validPage = pages[1];
        if (validPage.fields.size() != 2)
        {
            return Fail("valid page should keep fields");
        }

        if (validPage.fields[0].key != L"appearance.scale")
        {
            return Fail("fields should be sorted by order");
        }

        if (validPage.fields[1].defaultValue != L"light")
        {
            return Fail("enum default should normalize to first option when invalid");
        }

        return 0;
    }

    int TestPluginAndFenceRegistries()
    {
        PluginRegistry pluginRegistry;
        PluginStatus status;
        status.manifest.id = L"plugin.one";
        status.manifest.displayName = L"Plugin One";
        status.manifest.version = L"0.0.011";
        pluginRegistry.Upsert(status);

        const auto* found = pluginRegistry.FindById(L"plugin.one");
        if (!found)
        {
            return Fail("plugin should be findable by id");
        }

        pluginRegistry.Clear();
        if (!pluginRegistry.GetAll().empty())
        {
            return Fail("plugin registry clear should remove entries");
        }

        FenceExtensionRegistry fenceRegistry;
        fenceRegistry.RegisterContentProvider(FenceContentProviderDescriptor{L"custom.provider", L"file_collection", L"Custom"});

        if (!fenceRegistry.HasProvider(L"file_collection", L"custom.provider"))
        {
            return Fail("custom fence provider should be registered");
        }

        const auto fallback = fenceRegistry.ResolveOrDefault(L"missing_type", L"missing.provider");
        if (fallback.providerId != L"core.file_collection")
        {
            return Fail("unknown provider should resolve to core fallback");
        }

        return 0;
    }

    int TestPersistenceCorruptRecovery()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleFencesTests" / L"PersistenceCorruptRecovery";
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec)
        {
            return Fail("failed to create temp test directory");
        }

        const fs::path metadataPath = tempRoot / L"config.json";
        {
            std::ofstream out(metadataPath, std::ios::binary | std::ios::trunc);
            out << "{ malformed json";
        }

        Persistence persistence(metadataPath.wstring());
        std::vector<FenceModel> fences;
        if (!persistence.LoadFences(fences))
        {
            return Fail("LoadFences should recover from malformed metadata");
        }

        if (!fences.empty())
        {
            return Fail("recovered malformed metadata should yield empty fences");
        }

        if (fs::exists(metadataPath))
        {
            return Fail("corrupt metadata file should be quarantined");
        }

        bool foundBackup = false;
        for (const auto& entry : fs::directory_iterator(tempRoot, ec))
        {
            const std::wstring name = entry.path().filename().wstring();
            if (name.find(L"config.json.corrupt-") == 0)
            {
                foundBackup = true;
                break;
            }
        }

        if (!foundBackup)
        {
            return Fail("corrupt metadata quarantine backup file not found");
        }

        return 0;
    }

    int TestThemePlatformCustomPaletteCoverage()
    {
        SettingsStore store;
        store.Set(L"appearance.theme.mode", L"dark");
        store.Set(L"appearance.theme.style", L"custom");
        store.Set(L"appearance.theme.custom.window", L"#102030");
        store.Set(L"appearance.theme.custom.surface", L"#203040");
        store.Set(L"appearance.theme.custom.nav", L"#304050");
        store.Set(L"appearance.theme.custom.text", L"#405060");
        store.Set(L"appearance.theme.custom.subtle_text", L"#506070");
        store.Set(L"appearance.theme.custom.accent", L"#607080");
        store.Set(L"appearance.theme.custom.border", L"#708090");
        store.Set(L"appearance.theme.custom.fence_title_bar", L"#8090A0");
        store.Set(L"appearance.theme.custom.fence_title_text", L"#90A0B0");
        store.Set(L"appearance.theme.custom.fence_item_text", L"#A0B0C0");
        store.Set(L"appearance.theme.custom.fence_item_hover", L"#B0C0D0");

        ThemePlatform platform(&store);
        const ThemePalette palette = platform.BuildPalette();

        if (palette.windowColor != RGB(0x10, 0x20, 0x30) ||
            palette.surfaceColor != RGB(0x20, 0x30, 0x40) ||
            palette.navColor != RGB(0x30, 0x40, 0x50) ||
            palette.textColor != RGB(0x40, 0x50, 0x60) ||
            palette.subtleTextColor != RGB(0x50, 0x60, 0x70) ||
            palette.accentColor != RGB(0x60, 0x70, 0x80) ||
            palette.borderColor != RGB(0x70, 0x80, 0x90) ||
            palette.fenceTitleBarColor != RGB(0x80, 0x90, 0xA0) ||
            palette.fenceTitleTextColor != RGB(0x90, 0xA0, 0xB0) ||
            palette.fenceItemTextColor != RGB(0xA0, 0xB0, 0xC0) ||
            palette.fenceItemHoverColor != RGB(0xB0, 0xC0, 0xD0))
        {
            return Fail("custom theme palette should honor all exposed color tokens");
        }

        return 0;
    }

    class FakeApplicationCommands final : public IApplicationCommands
    {
    public:
        std::wstring CreateFenceNearCursor() override
        {
            lastCreateRequest = FenceCreateRequest{};
            return createdFenceId;
        }

        std::wstring CreateFenceNearCursor(const FenceCreateRequest& request) override
        {
            lastCreateRequest = request;
            return createdFenceId;
        }
        void ExitApplication() override {}
        void OpenSettings() override {}

        CommandContext GetCurrentCommandContext() const override
        {
            return currentCommandContext;
        }

        FenceMetadata GetActiveFenceMetadata() const override
        {
            return active;
        }

        std::vector<std::wstring> GetAllFenceIds() const override
        {
            return allFenceIds;
        }

        FenceMetadata GetFenceMetadata(const std::wstring& fenceId) const override
        {
            for (const auto& meta : knownFences)
            {
                if (meta.id == fenceId)
                {
                    return meta;
                }
            }
            return {};
        }

        void RefreshFence(const std::wstring& fenceId) override
        {
            ++refreshCalls;
            lastRefreshedFence = fenceId;
        }

        void UpdateFenceContentSource(const std::wstring& fenceId, const std::wstring& contentSource) override
        {
            lastUpdatedFenceSourceFenceId = fenceId;
            lastUpdatedFenceSource = contentSource;
        }

        void UpdateFenceContentState(const std::wstring& fenceId,
                                     const std::wstring& state,
                                     const std::wstring& detail) override
        {
            lastUpdatedFenceStateFenceId = fenceId;
            lastUpdatedFenceState = state;
            lastUpdatedFenceStateDetail = detail;
        }

        void UpdateFencePresentation(const std::wstring& fenceId,
                                     const FencePresentationSettings& settings) override
        {
            lastPresentationFenceId = fenceId;
            lastPresentationSettings = settings;
        }

        FenceCreateRequest lastCreateRequest;
        std::wstring createdFenceId = L"created.fence";
        FenceMetadata active;
        std::vector<FenceMetadata> knownFences;
        std::vector<std::wstring> allFenceIds;
        CommandContext currentCommandContext;
        int refreshCalls = 0;
        std::wstring lastRefreshedFence;
        std::wstring lastUpdatedFenceSourceFenceId;
        std::wstring lastUpdatedFenceSource;
        std::wstring lastUpdatedFenceStateFenceId;
        std::wstring lastUpdatedFenceState;
        std::wstring lastUpdatedFenceStateDetail;
        std::wstring lastPresentationFenceId;
        FencePresentationSettings lastPresentationSettings;
    };

    std::unique_ptr<IPlugin> FindBuiltinPluginById(const std::wstring& pluginId)
    {
        auto plugins = CreateBuiltinPlugins();
        for (auto& plugin : plugins)
        {
            if (plugin && plugin->GetManifest().id == pluginId)
            {
                return std::move(plugin);
            }
        }
        return nullptr;
    }

    int TestFenceMetadataRefreshContract()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleFencesTests" / L"FenceMetadataRefreshContract";
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec)
        {
            return Fail("failed to create fence metadata contract test directory");
        }

        const fs::path fileTxt = tempRoot / L"alpha.txt";
        const fs::path filePng = tempRoot / L"image.png";
        {
            std::ofstream out(fileTxt, std::ios::binary | std::ios::trunc);
            out << "alpha";
        }
        {
            std::ofstream out(filePng, std::ios::binary | std::ios::trunc);
            out << "image";
        }

        CommandDispatcher dispatcher;
        PluginSettingsRegistry settingsRegistry;
        MenuContributionRegistry menuRegistry;
        Diagnostics diagnostics;
        FakeApplicationCommands appCommands;

        appCommands.active = FenceMetadata{L"fence.contract", L"Contract Fence", tempRoot.wstring()};
        appCommands.knownFences.push_back(appCommands.active);
        appCommands.allFenceIds.push_back(appCommands.active.id);

        auto organizer = FindBuiltinPluginById(L"builtin.fence_organizer");
        if (!organizer)
        {
            return Fail("builtin.fence_organizer should exist");
        }

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.appCommands = &appCommands;

        if (!organizer->Initialize(context))
        {
            return Fail("fence organizer plugin should initialize");
        }

        if (!dispatcher.HasCommand(L"organizer.by_type") ||
            !dispatcher.HasCommand(L"organizer.flatten") ||
            !dispatcher.HasCommand(L"organizer.cleanup_empty"))
        {
            return Fail("fence organizer commands should be registered");
        }

        const auto fenceMenu = menuRegistry.GetBySurface(MenuSurface::FenceContext);
        bool foundByType = false;
        bool foundFlatten = false;
        bool foundCleanup = false;
        for (const auto& item : fenceMenu)
        {
            foundByType = foundByType || item.commandId == L"organizer.by_type";
            foundFlatten = foundFlatten || item.commandId == L"organizer.flatten";
            foundCleanup = foundCleanup || item.commandId == L"organizer.cleanup_empty";
        }
        if (!foundByType || !foundFlatten || !foundCleanup)
        {
            return Fail("fence organizer menu contributions should be registered");
        }

        if (!dispatcher.Dispatch(L"organizer.by_type"))
        {
            return Fail("organizer.by_type should dispatch");
        }

        const fs::path typeTxt = tempRoot / L"_type_txt" / L"alpha.txt";
        const fs::path typePng = tempRoot / L"_type_png" / L"image.png";
        if (!fs::exists(typeTxt) || !fs::exists(typePng))
        {
            return Fail("organizer.by_type should move files into type folders");
        }
        if (appCommands.refreshCalls != 1 || appCommands.lastRefreshedFence != L"fence.contract")
        {
            return Fail("organizer.by_type should call RefreshFence for active fence");
        }

        if (!dispatcher.Dispatch(L"organizer.flatten"))
        {
            return Fail("organizer.flatten should dispatch");
        }
        if (!fs::exists(tempRoot / L"alpha.txt") || !fs::exists(tempRoot / L"image.png"))
        {
            return Fail("organizer.flatten should move files back to fence root");
        }
        if (appCommands.refreshCalls != 2)
        {
            return Fail("organizer.flatten should call RefreshFence");
        }

        if (!dispatcher.Dispatch(L"organizer.cleanup_empty"))
        {
            return Fail("organizer.cleanup_empty should dispatch");
        }
        if (fs::exists(tempRoot / L"_type_txt") || fs::exists(tempRoot / L"_type_png"))
        {
            return Fail("organizer.cleanup_empty should remove empty organizer folders");
        }
        if (appCommands.refreshCalls != 3)
        {
            return Fail("organizer.cleanup_empty should call RefreshFence");
        }

        // No active fence should fail gracefully and avoid refresh side effects.
        appCommands.active = {};
        const int refreshBefore = appCommands.refreshCalls;
        if (!dispatcher.Dispatch(L"organizer.by_type"))
        {
            return Fail("organizer.by_type command dispatch should remain handled without active fence");
        }
        if (appCommands.refreshCalls != refreshBefore)
        {
            return Fail("organizer.by_type should not refresh when active fence metadata is missing");
        }

        organizer->Shutdown();
        return 0;
    }

    int TestFolderPortalProviderAndVisualModes()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleFencesTests" / L"FolderPortalProvider";
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec)
        {
            return Fail("failed to create folder portal temp directory");
        }

        {
            std::ofstream out(tempRoot / L"beta.txt", std::ios::binary | std::ios::trunc);
            out << "beta";
        }

        CommandDispatcher dispatcher;
        PluginSettingsRegistry settingsRegistry;
        MenuContributionRegistry menuRegistry;
        FenceExtensionRegistry fenceRegistry;
        Diagnostics diagnostics;
        FakeApplicationCommands appCommands;

        FenceMetadata portalFence;
        portalFence.id = L"fence.portal";
        portalFence.title = L"Portal Fence";
        portalFence.backingFolderPath = tempRoot.wstring();
        portalFence.contentType = L"folder_portal";
        portalFence.contentPluginId = L"builtin.explorer_portal";
        portalFence.contentSource = tempRoot.wstring();
        appCommands.active = portalFence;
        appCommands.knownFences.push_back(portalFence);
        appCommands.allFenceIds.push_back(portalFence.id);

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.fenceExtensionRegistry = &fenceRegistry;
        context.appCommands = &appCommands;

        auto portal = FindBuiltinPluginById(L"builtin.explorer_portal");
        if (!portal || !portal->Initialize(context))
        {
            return Fail("builtin.explorer_portal should initialize");
        }

        const auto* callbacks = fenceRegistry.ResolveCallbacks(L"folder_portal", L"builtin.explorer_portal");
        if (!callbacks || !callbacks->enumerateItems)
        {
            portal->Shutdown();
            return Fail("folder portal provider callbacks should be registered");
        }

        const auto items = callbacks->enumerateItems(portalFence);
        if (items.size() != 1 || items[0].name != L"beta.txt")
        {
            portal->Shutdown();
            return Fail("folder portal provider should enumerate source items");
        }

        portal->Shutdown();

        auto appearance = FindBuiltinPluginById(L"builtin.appearance");
        if (!appearance || !appearance->Initialize(context))
        {
            return Fail("builtin.appearance should initialize");
        }

        CommandContext appearanceContext;
        appearanceContext.commandId = L"appearance.mode.focus";
        appearanceContext.invocationSource = L"fence_context";
        appearanceContext.fence.id = portalFence.id;

        if (!dispatcher.Dispatch(L"appearance.mode.focus", appearanceContext))
        {
            appearance->Shutdown();
            return Fail("appearance.mode.focus should dispatch");
        }

        if (appCommands.lastPresentationFenceId != portalFence.id)
        {
            appearance->Shutdown();
            return Fail("appearance mode should target routed fence context");
        }

        if (!appCommands.lastPresentationSettings.textOnlyMode.has_value() ||
            appCommands.lastPresentationSettings.textOnlyMode.value() != false ||
            !appCommands.lastPresentationSettings.iconSpacingPreset.has_value() ||
            appCommands.lastPresentationSettings.iconSpacingPreset.value() != L"spacious")
        {
            appearance->Shutdown();
            return Fail("appearance focus mode should apply expected presentation settings");
        }

        appearance->Shutdown();
        return 0;
    }
}

int main()
{
    if (const int result = TestCommandDispatcher(); result != 0)
    {
        return result;
    }

    if (const int result = TestMenuRegistry(); result != 0)
    {
        return result;
    }

    if (const int result = TestSettingsRegistry(); result != 0)
    {
        return result;
    }

    if (const int result = TestPluginAndFenceRegistries(); result != 0)
    {
        return result;
    }

    if (const int result = TestPersistenceCorruptRecovery(); result != 0)
    {
        return result;
    }

    if (const int result = TestThemePlatformCustomPaletteCoverage(); result != 0)
    {
        return result;
    }

    if (const int result = TestFenceMetadataRefreshContract(); result != 0)
    {
        return result;
    }

    if (const int result = TestFolderPortalProviderAndVisualModes(); result != 0)
    {
        return result;
    }

    std::cout << "HostCoreTests passed\n";
    return 0;
}
