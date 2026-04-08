#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "core/SettingsStore.h"
#include "core/ThemePlatform.h"
#include "Persistence.h"
#include "plugins/builtins/BuiltinPlugins.h"
#include "extensions/PluginContracts.h"
#include "extensions/SpaceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginRegistry.h"
#include "extensions/PluginSettingsRegistry.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int RunPluginContractValidationTests();
int RunPluginUpdaterGatesTests();
int RunThemeTokenPolicyTests();
int RunPluginSecurityPolicyTests();
int RunThemeMigrationTests();
int RunThemeRenderingConsistencyTests();
int RunThemeFallbackTests();
int RunThemePersistenceTests();
int RunThemeApplyIntegrationTests();
int RunThemeTokenResolverIntegrationTests();
int RunThemeFullLifecycleTests();
int RunThemePackageValidationIntegrationTests();
int RunThemeFailureFocusedTests();
int RunPluginLoaderConflictIntegrationTests();
int RunPluginHostRuntimeConflictTests();
int RunTelemetrySnapshotDiagnostics();

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
        context.space.id = L"space.context";
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

        if (receivedContext.space.id != L"space.context" || receivedContext.invocationSource != L"test")
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

    int TestPluginAndSpaceRegistries()
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

        SpaceExtensionRegistry spaceRegistry;
        spaceRegistry.RegisterContentProvider(SpaceContentProviderDescriptor{L"custom.provider", L"file_collection", L"Custom"});

        if (!spaceRegistry.HasProvider(L"file_collection", L"custom.provider"))
        {
            return Fail("custom space provider should be registered");
        }

        const auto fallback = spaceRegistry.ResolveOrDefault(L"missing_type", L"missing.provider");
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
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleSpacesTests" / L"PersistenceCorruptRecovery";
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
        std::vector<SpaceModel> spaces;
        if (!persistence.LoadSpaces(spaces))
        {
            return Fail("LoadSpaces should recover from malformed metadata");
        }

        if (!spaces.empty())
        {
            return Fail("recovered malformed metadata should yield empty spaces");
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
        store.Set(L"theme.source", L"win32_theme_system");
        store.Set(L"theme.win32.theme_id", L"nocturne-dark");

        // Legacy custom color keys should no longer drive palette output.
        store.Set(L"appearance.theme.custom.window", L"#102030");
        store.Set(L"appearance.theme.custom.surface", L"#203040");
        store.Set(L"appearance.theme.custom.nav", L"#304050");
        store.Set(L"appearance.theme.custom.text", L"#405060");
        store.Set(L"appearance.theme.custom.subtle_text", L"#506070");
        store.Set(L"appearance.theme.custom.accent", L"#607080");
        store.Set(L"appearance.theme.custom.border", L"#708090");
        store.Set(L"appearance.theme.custom.space_title_bar", L"#8090A0");
        store.Set(L"appearance.theme.custom.space_title_text", L"#90A0B0");
        store.Set(L"appearance.theme.custom.space_item_text", L"#A0B0C0");
        store.Set(L"appearance.theme.custom.space_item_hover", L"#B0C0D0");

        ThemePlatform platform(&store);
        const ThemePalette palette = platform.BuildPalette();

        if (palette.windowColor != RGB(34, 39, 46) ||
            palette.surfaceColor != RGB(44, 50, 58) ||
            palette.navColor != RGB(28, 33, 40) ||
            palette.textColor != RGB(173, 186, 199) ||
            palette.subtleTextColor != RGB(118, 131, 144) ||
            palette.accentColor != RGB(109, 96, 178))
        {
            return Fail("win32 theme palette should be selected from canonical theme.win32.theme_id");
        }

        if (palette.windowColor == RGB(0x10, 0x20, 0x30) ||
            palette.surfaceColor == RGB(0x20, 0x30, 0x40) ||
            palette.accentColor == RGB(0x60, 0x70, 0x80))
        {
            return Fail("legacy custom color keys should not override win32 theme palette");
        }

        return 0;
    }

    class FakeApplicationCommands final : public IApplicationCommands
    {
    public:
        std::wstring CreateSpaceNearCursor() override
        {
            lastCreateRequest = SpaceCreateRequest{};
            return createdSpaceId;
        }

        std::wstring CreateSpaceNearCursor(const SpaceCreateRequest& request) override
        {
            lastCreateRequest = request;
            return createdSpaceId;
        }
        void ExitApplication() override {}
        void OpenSettings() override {}

        CommandContext GetCurrentCommandContext() const override
        {
            return currentCommandContext;
        }

        SpaceMetadata GetActiveSpaceMetadata() const override
        {
            return active;
        }

        std::vector<std::wstring> GetAllSpaceIds() const override
        {
            return allSpaceIds;
        }

        SpaceMetadata GetSpaceMetadata(const std::wstring& spaceId) const override
        {
            for (const auto& meta : knownSpaces)
            {
                if (meta.id == spaceId)
                {
                    return meta;
                }
            }
            return {};
        }

        void RefreshSpace(const std::wstring& spaceId) override
        {
            ++refreshCalls;
            lastRefreshedSpace = spaceId;
        }

        void UpdateSpaceContentSource(const std::wstring& spaceId, const std::wstring& contentSource) override
        {
            lastUpdatedSpaceSourceSpaceId = spaceId;
            lastUpdatedSpaceSource = contentSource;
        }

        void UpdateSpaceContentState(const std::wstring& spaceId,
                                     const std::wstring& state,
                                     const std::wstring& detail) override
        {
            lastUpdatedSpaceStateSpaceId = spaceId;
            lastUpdatedSpaceState = state;
            lastUpdatedSpaceStateDetail = detail;
        }

        void UpdateSpacePresentation(const std::wstring& spaceId,
                                     const SpacePresentationSettings& settings) override
        {
            lastPresentationSpaceId = spaceId;
            lastPresentationSettings = settings;
        }

        SpaceCreateRequest lastCreateRequest;
        std::wstring createdSpaceId = L"created.space";
        SpaceMetadata active;
        std::vector<SpaceMetadata> knownSpaces;
        std::vector<std::wstring> allSpaceIds;
        CommandContext currentCommandContext;
        int refreshCalls = 0;
        std::wstring lastRefreshedSpace;
        std::wstring lastUpdatedSpaceSourceSpaceId;
        std::wstring lastUpdatedSpaceSource;
        std::wstring lastUpdatedSpaceStateSpaceId;
        std::wstring lastUpdatedSpaceState;
        std::wstring lastUpdatedSpaceStateDetail;
        std::wstring lastPresentationSpaceId;
        SpacePresentationSettings lastPresentationSettings;
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

    int TestSpaceMetadataRefreshContract()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleSpacesTests" / L"SpaceMetadataRefreshContract";
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        if (ec)
        {
            return Fail("failed to create space metadata contract test directory");
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

        appCommands.active = SpaceMetadata{L"space.contract", L"Contract Space", tempRoot.wstring()};
        appCommands.knownSpaces.push_back(appCommands.active);
        appCommands.allSpaceIds.push_back(appCommands.active.id);

        auto organizer = FindBuiltinPluginById(L"builtin.space_organizer");
        if (!organizer)
        {
            return Fail("builtin.space_organizer should exist");
        }

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.appCommands = &appCommands;

        if (!organizer->Initialize(context))
        {
            return Fail("space organizer plugin should initialize");
        }

        if (!dispatcher.HasCommand(L"organizer.by_type") ||
            !dispatcher.HasCommand(L"organizer.flatten") ||
            !dispatcher.HasCommand(L"organizer.cleanup_empty"))
        {
            return Fail("space organizer commands should be registered");
        }

        const auto spaceMenu = menuRegistry.GetBySurface(MenuSurface::SpaceContext);
        bool foundByType = false;
        bool foundFlatten = false;
        bool foundCleanup = false;
        for (const auto& item : spaceMenu)
        {
            foundByType = foundByType || item.commandId == L"organizer.by_type";
            foundFlatten = foundFlatten || item.commandId == L"organizer.flatten";
            foundCleanup = foundCleanup || item.commandId == L"organizer.cleanup_empty";
        }
        if (!foundByType || !foundFlatten || !foundCleanup)
        {
            return Fail("space organizer menu contributions should be registered");
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
        if (appCommands.refreshCalls != 1 || appCommands.lastRefreshedSpace != L"space.contract")
        {
            return Fail("organizer.by_type should call RefreshSpace for active space");
        }

        if (!dispatcher.Dispatch(L"organizer.flatten"))
        {
            return Fail("organizer.flatten should dispatch");
        }
        if (!fs::exists(tempRoot / L"alpha.txt") || !fs::exists(tempRoot / L"image.png"))
        {
            return Fail("organizer.flatten should move files back to space root");
        }
        if (appCommands.refreshCalls != 2)
        {
            return Fail("organizer.flatten should call RefreshSpace");
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
            return Fail("organizer.cleanup_empty should call RefreshSpace");
        }

        // No active space should fail gracefully and avoid refresh side effects.
        appCommands.active = {};
        const int refreshBefore = appCommands.refreshCalls;
        if (!dispatcher.Dispatch(L"organizer.by_type"))
        {
            return Fail("organizer.by_type command dispatch should remain handled without active space");
        }
        if (appCommands.refreshCalls != refreshBefore)
        {
            return Fail("organizer.by_type should not refresh when active space metadata is missing");
        }

        organizer->Shutdown();
        return 0;
    }

    int TestFolderPortalProviderAndVisualModes()
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec) / L"SimpleSpacesTests" / L"FolderPortalProvider";
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
        SpaceExtensionRegistry spaceRegistry;
        Diagnostics diagnostics;
        FakeApplicationCommands appCommands;

        SpaceMetadata portalSpace;
        portalSpace.id = L"space.portal";
        portalSpace.title = L"Portal Space";
        portalSpace.backingFolderPath = tempRoot.wstring();
        portalSpace.contentType = L"folder_portal";
        portalSpace.contentPluginId = L"builtin.explorer_portal";
        portalSpace.contentSource = tempRoot.wstring();
        appCommands.active = portalSpace;
        appCommands.knownSpaces.push_back(portalSpace);
        appCommands.allSpaceIds.push_back(portalSpace.id);

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.spaceExtensionRegistry = &spaceRegistry;
        context.appCommands = &appCommands;

        auto portal = FindBuiltinPluginById(L"builtin.explorer_portal");
        if (!portal || !portal->Initialize(context))
        {
            return Fail("builtin.explorer_portal should initialize");
        }

        const auto* callbacks = spaceRegistry.ResolveCallbacks(L"folder_portal", L"builtin.explorer_portal");
        if (!callbacks || !callbacks->enumerateItems)
        {
            portal->Shutdown();
            return Fail("folder portal provider callbacks should be registered");
        }

        const auto items = callbacks->enumerateItems(portalSpace);
        if (items.size() != 1 || items[0].name != L"beta.txt")
        {
            portal->Shutdown();
            return Fail("folder portal provider should enumerate source items");
        }

        portal->Shutdown();

        auto appearance = FindBuiltinPluginById(L"community.visual_modes");
        if (!appearance || !appearance->Initialize(context))
        {
            return Fail("community.visual_modes should initialize");
        }

        CommandContext appearanceContext;
        appearanceContext.commandId = L"appearance.mode.focus";
        appearanceContext.invocationSource = L"space_context";
        appearanceContext.space.id = portalSpace.id;

        if (!dispatcher.Dispatch(L"appearance.mode.focus", appearanceContext))
        {
            appearance->Shutdown();
            return Fail("appearance.mode.focus should dispatch");
        }

        if (appCommands.lastPresentationSpaceId != portalSpace.id)
        {
            appearance->Shutdown();
            return Fail("appearance mode should target routed space context");
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

    if (const int result = TestPluginAndSpaceRegistries(); result != 0)
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

    if (const int result = TestSpaceMetadataRefreshContract(); result != 0)
    {
        return result;
    }

    if (const int result = TestFolderPortalProviderAndVisualModes(); result != 0)
    {
        return result;
    }

    if (const int result = RunPluginContractValidationTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunPluginUpdaterGatesTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemeTokenPolicyTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunPluginSecurityPolicyTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemeMigrationTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemeRenderingConsistencyTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemeFallbackTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemePersistenceTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunThemeFailureFocusedTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunPluginLoaderConflictIntegrationTests(); result != 0)
    {
        return result;
    }

    if (const int result = RunPluginHostRuntimeConflictTests(); result != 0)
    {
        return result;
    }
        if (const int result = RunThemeApplyIntegrationTests(); result != 0)
        {
            return result;
        }

        if (const int result = RunThemeTokenResolverIntegrationTests(); result != 0)
        {
            return result;
        }

        if (const int result = RunThemeFullLifecycleTests(); result != 0)
        {
            return result;
        }

        if (const int result = RunThemePackageValidationIntegrationTests(); result != 0)
        {
            return result;
        }

    if (const int result = RunTelemetrySnapshotDiagnostics(); result != 0)
    {
        return result;
    }

    std::cout << "HostCoreTests passed\n";

    return 0;
}

