#include "plugins/builtins/BuiltinPlugins.h"

#include "extensions/FenceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"

class CoreCommandsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.core_commands";
        manifest.displayName = L"Core Commands";
        manifest.version = L"0.0.010";
        manifest.description = L"Registers stable core command routes and core file_collection provider.";
        manifest.capabilities = {L"commands", L"fence_content_provider"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true});
        }

        if (context.settingsRegistry)
        {
            // --- Behavior page ---
            PluginSettingsPage behavior;
            behavior.pluginId = L"builtin.core_commands";
            behavior.pageId   = L"core.behavior";
            behavior.title    = L"Behavior";
            behavior.order    = 10;

            SettingsFieldDescriptor autoFocus;
            autoFocus.key          = L"core.behavior.auto_focus_on_create";
            autoFocus.label        = L"Auto-focus new fence";
            autoFocus.description  = L"Show and focus the fence window immediately after creation.";
            autoFocus.type         = SettingsFieldType::Bool;
            autoFocus.defaultValue = L"true";
            autoFocus.order        = 10;
            behavior.fields.push_back(std::move(autoFocus));

            SettingsFieldDescriptor defaultTitle;
            defaultTitle.key          = L"core.behavior.default_title";
            defaultTitle.label        = L"Default fence title";
            defaultTitle.description  = L"Title template applied to newly created fences.";
            defaultTitle.type         = SettingsFieldType::String;
            defaultTitle.defaultValue = L"New Fence";
            defaultTitle.order        = 20;
            behavior.fields.push_back(std::move(defaultTitle));

            context.settingsRegistry->RegisterPage(std::move(behavior));

            // --- Content Provider page ---
            PluginSettingsPage provider;
            provider.pluginId = L"builtin.core_commands";
            provider.pageId   = L"core.provider";
            provider.title    = L"Content Provider";
            provider.order    = 20;

            SettingsFieldDescriptor strictMode;
            strictMode.key          = L"core.provider.strict_mode";
            strictMode.label        = L"Strict provider mode";
            strictMode.description  = L"Reject unrecognised provider ids instead of normalising to core.file_collection.";
            strictMode.type         = SettingsFieldType::Bool;
            strictMode.defaultValue = L"false";
            strictMode.order        = 10;
            provider.fields.push_back(std::move(strictMode));

            context.settingsRegistry->RegisterPage(std::move(provider));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class TrayPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.tray";
        manifest.displayName = L"Tray Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Contributes tray menu items routed through commands.";
        manifest.capabilities = {L"tray_contributions", L"commands"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (!context.menuRegistry)
        {
            return false;
        }

        if (context.settingsRegistry)
        {
            PluginSettingsPage trayPage;
            trayPage.pluginId = L"builtin.tray";
            trayPage.pageId   = L"tray.behavior";
            trayPage.title    = L"Tray Behavior";
            trayPage.order    = 10;

            SettingsFieldDescriptor closeToTray;
            closeToTray.key          = L"tray.behavior.close_to_tray";
            closeToTray.label        = L"Close to tray";
            closeToTray.description  = L"Closing the main window hides it instead of exiting.";
            closeToTray.type         = SettingsFieldType::Bool;
            closeToTray.defaultValue = L"true";
            closeToTray.order        = 10;
            trayPage.fields.push_back(std::move(closeToTray));

            SettingsFieldDescriptor startMinimized;
            startMinimized.key          = L"tray.behavior.start_minimized";
            startMinimized.label        = L"Start minimized";
            startMinimized.description  = L"Launch the app without showing any window; only the tray icon appears.";
            startMinimized.type         = SettingsFieldType::Bool;
            startMinimized.defaultValue = L"false";
            startMinimized.order        = 20;
            trayPage.fields.push_back(std::move(startMinimized));

            context.settingsRegistry->RegisterPage(std::move(trayPage));
        }

        context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"New Fence", L"fence.create", 10, false});
        context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"Exit", L"app.exit", 1000, true});
        return true;
    }

    void Shutdown() override
    {
    }
};

class SettingsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.settings";
        manifest.displayName = L"Settings Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Registers settings-page scaffolding.";
        manifest.capabilities = {L"settings_pages", L"menu_contributions"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (!context.settingsRegistry)
        {
            return false;
        }

        context.settingsRegistry->RegisterPage(PluginSettingsPage{L"builtin.settings", L"general", L"General", 10});
        context.settingsRegistry->RegisterPage(PluginSettingsPage{L"builtin.settings", L"plugins", L"Plugins", 20});
        context.settingsRegistry->RegisterPage(PluginSettingsPage{L"builtin.settings", L"diagnostics", L"Diagnostics", 30});

        if (context.menuRegistry)
        {
            context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"Settings", L"plugin.openSettings", 900, false});
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class AppearancePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.appearance";
        manifest.displayName = L"Appearance Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Placeholder for theme and appearance contributions.";
        manifest.capabilities = {L"appearance", L"settings_pages"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.settingsRegistry)
        {
            context.settingsRegistry->RegisterPage(PluginSettingsPage{L"builtin.appearance", L"appearance.theme", L"Theme", 10,
            {
                SettingsFieldDescriptor{
                    L"appearance.theme.mode",
                    L"Theme mode",
                    L"Follow the Windows system preference, or force a fixed theme.",
                    SettingsFieldType::Enum,
                    L"system",
                    {
                        SettingsEnumOption{L"system", L"Follow system"},
                        SettingsEnumOption{L"light",  L"Always light"},
                        SettingsEnumOption{L"dark",   L"Always dark"},
                    },
                    10
                },
                SettingsFieldDescriptor{
                    L"appearance.theme.use_accent",
                    L"Use system accent colour",
                    L"Apply the Windows accent colour to fence header bars.",
                    SettingsFieldType::Bool,
                    L"false",
                    {},
                    20
                },
            }});
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class ExplorerFencePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.explorer_portal";
        manifest.displayName = L"Explorer Fence Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Placeholder for folder portal fences.";
        manifest.capabilities = {L"fence_content_provider", L"settings_pages"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"builtin.explorer_portal", L"folder_portal", L"Folder Portal"});
        }

        if (context.settingsRegistry)
        {
            PluginSettingsPage portalPage;
            portalPage.pluginId = L"builtin.explorer_portal";
            portalPage.pageId   = L"explorer.portal";
            portalPage.title    = L"Portal Behavior";
            portalPage.order    = 10;

            portalPage.fields.push_back(SettingsFieldDescriptor{
                L"explorer.portal.recurse_subfolders",
                L"Include subfolders",
                L"Show items from subdirectories in the folder portal fence.",
                SettingsFieldType::Bool, L"false", {}, 10});
            portalPage.fields.push_back(SettingsFieldDescriptor{
                L"explorer.portal.show_hidden",
                L"Show hidden files",
                L"Include files and folders marked as Hidden.",
                SettingsFieldType::Bool, L"false", {}, 20});

            context.settingsRegistry->RegisterPage(std::move(portalPage));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class WidgetsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.widgets";
        manifest.displayName = L"Widgets Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Placeholder for widget panel fences.";
        manifest.capabilities = {L"widgets", L"fence_content_provider"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"builtin.widgets", L"widget_panel", L"Widget Panel"});
        }

        if (context.settingsRegistry)
        {
            // Widget Layout page
            PluginSettingsPage layoutPage;
            layoutPage.pluginId = L"builtin.widgets";
            layoutPage.pageId   = L"widgets.layout";
            layoutPage.title    = L"Widget Layout";
            layoutPage.order    = 10;

            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.compact_mode",
                L"Compact card mode",
                L"Display widget cards in a denser compact layout.",
                SettingsFieldType::Bool, L"false", {}, 10});
            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.snap_to_grid",
                L"Snap to grid",
                L"Snap widget positions to an invisible alignment grid.",
                SettingsFieldType::Bool, L"true", {}, 20});
            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.grid_size",
                L"Grid size (px)",
                L"Pixel size of the snap grid when snap-to-grid is active.",
                SettingsFieldType::Int, L"16", {}, 30});

            context.settingsRegistry->RegisterPage(std::move(layoutPage));

            // Refresh Policy page
            PluginSettingsPage refreshPage;
            refreshPage.pluginId = L"builtin.widgets";
            refreshPage.pageId   = L"widgets.refresh";
            refreshPage.title    = L"Refresh Policy";
            refreshPage.order    = 20;

            refreshPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.refresh.interval_seconds",
                L"Refresh interval (s)",
                L"How often widget content is refreshed. Set 0 to disable automatic refresh.",
                SettingsFieldType::Int, L"60", {}, 10});
            refreshPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.refresh.pause_on_battery",
                L"Pause on battery",
                L"Stop automatic refreshes when the device is running on battery power.",
                SettingsFieldType::Bool, L"true", {}, 20});

            context.settingsRegistry->RegisterPage(std::move(refreshPage));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class DesktopContextPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.desktop_context";
        manifest.displayName = L"Desktop Context Plugin";
        manifest.version = L"0.0.010";
        manifest.description = L"Placeholder for desktop context integration path.";
        manifest.capabilities = {L"desktop_context", L"settings_pages"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.settingsRegistry)
        {
            PluginSettingsPage ctxPage;
            ctxPage.pluginId = L"builtin.desktop_context";
            ctxPage.pageId   = L"desktop.context";
            ctxPage.title    = L"Context Actions";
            ctxPage.order    = 10;

            ctxPage.fields.push_back(SettingsFieldDescriptor{
                L"desktop.context.show_quick_actions",
                L"Show fence quick actions",
                L"Display context-menu quick actions when right-clicking a fence.",
                SettingsFieldType::Bool, L"true", {}, 10});
            ctxPage.fields.push_back(SettingsFieldDescriptor{
                L"desktop.context.safety_confirmations",
                L"Confirm destructive actions",
                L"Ask for confirmation before deleting or clearing a fence.",
                SettingsFieldType::Bool, L"true", {}, 20});

            context.settingsRegistry->RegisterPage(std::move(ctxPage));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

std::vector<std::unique_ptr<IPlugin>> CreateBuiltinPlugins()
{
    std::vector<std::unique_ptr<IPlugin>> plugins;
    plugins.push_back(std::make_unique<CoreCommandsPlugin>());
    plugins.push_back(std::make_unique<TrayPlugin>());
    plugins.push_back(std::make_unique<SettingsPlugin>());
    plugins.push_back(std::make_unique<AppearancePlugin>());
    plugins.push_back(std::make_unique<ExplorerFencePlugin>());
    plugins.push_back(std::make_unique<WidgetsPlugin>());
    plugins.push_back(std::make_unique<DesktopContextPlugin>());
    return plugins;
}
