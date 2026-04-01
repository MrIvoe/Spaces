#include "NetworkDriveFencePlugin.h"

#include "extensions/FenceExtensionRegistry.h"
#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"

PluginManifest NetworkDriveFencePlugin::GetManifest() const
{
    PluginManifest m;
    m.id           = L"community.network_drive_fence";
    m.displayName  = L"Network Drive Fence";
    m.version      = L"1.0.0";
    m.description  = L"Fence content provider for UNC paths and mapped network drives.";
    m.capabilities = {L"fence_content_provider", L"settings_pages"};
    return m;
}

bool NetworkDriveFencePlugin::Initialize(const PluginContext& context)
{
    // Register the content provider type so fences can use it.
    if (context.fenceExtensionRegistry)
    {
        FenceContentProviderDescriptor desc;
        desc.id          = L"community.network_drive_fence";
        desc.providerId  = L"network_drive";
        desc.displayName = L"Network Drive";
        desc.isDefault   = false;
        context.fenceExtensionRegistry->RegisterContentProvider(desc);
    }

    // Settings page
    if (context.settingsRegistry)
    {
        PluginSettingsPage page;
        page.pluginId = L"community.network_drive_fence";
        page.pageId   = L"net_fence.general";
        page.title    = L"Network Drive";
        page.order    = 10;

        page.fields.push_back(SettingsFieldDescriptor{
            L"net_fence.general.default_path",
            L"Default UNC path",
            L"Path pre-filled when creating a new network drive fence (e.g. \\\\server\\share).",
            SettingsFieldType::String, L"", {}, 10
        });

        page.fields.push_back(SettingsFieldDescriptor{
            L"net_fence.general.reconnect_on_load",
            L"Reconnect on startup",
            L"Attempt to re-connect disconnected network drives when SimpleFences launches.",
            SettingsFieldType::Bool, L"true", {}, 20
        });

        page.fields.push_back(SettingsFieldDescriptor{
            L"net_fence.general.offline_label",
            L"Offline mode",
            L"How to display a fence when the network path is unreachable.",
            SettingsFieldType::Enum, L"grayed",
            {
                {L"grayed",  L"Show greyed-out icon"},
                {L"hidden",  L"Hide fence"},
                {L"error",   L"Show error message"},
            },
            30
        });

        context.settingsRegistry->RegisterPage(std::move(page));
    }

    return true;
}

void NetworkDriveFencePlugin::Shutdown() {}
