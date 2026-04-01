#include "plugins/builtins/BuiltinPlugins.h"

#include "extensions/FenceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginSettingsRegistry.h"

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
        manifest.capabilities = {L"appearance"};
        return manifest;
    }

    bool Initialize(const PluginContext&) override
    {
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
        manifest.capabilities = {L"fence_content_provider"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"builtin.explorer_portal", L"folder_portal", L"Folder Portal"});
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
        manifest.capabilities = {L"desktop_context"};
        return manifest;
    }

    bool Initialize(const PluginContext&) override
    {
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
