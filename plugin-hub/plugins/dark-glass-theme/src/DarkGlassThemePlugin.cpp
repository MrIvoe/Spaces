#include "DarkGlassThemePlugin.h"

#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"

PluginManifest DarkGlassThemePlugin::GetManifest() const
{
    PluginManifest m;
    m.id             = L"community.dark_glass_theme";
    m.displayName    = L"Dark Glass Theme";
    m.version        = L"1.0.0";
    m.description    = L"Translucent dark theme for fence windows with a frosted-glass look.";
    m.enabledByDefault = true;
    m.capabilities   = {L"appearance", L"settings_pages"};
    return m;
}

bool DarkGlassThemePlugin::Initialize(const PluginContext& context)
{
    if (!context.settingsRegistry)
    {
        return true; // no registry – run with defaults
    }

    // --- Settings page ---
    PluginSettingsPage page;
    page.pluginId = L"community.dark_glass_theme";
    page.pageId   = L"dark_glass.style";
    page.title    = L"Glass Style";
    page.order    = 10;

    page.fields.push_back(SettingsFieldDescriptor{
        L"dark_glass.style.enabled",
        L"Enable dark glass theme",
        L"Apply the translucent dark style to all fence windows.",
        SettingsFieldType::Bool, L"true", {}, 10
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"dark_glass.style.opacity",
        L"Background opacity (0-100)",
        L"Percentage opacity of the fence background. 100 = fully opaque.",
        SettingsFieldType::Int, L"80", {}, 20
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"dark_glass.style.blur_mode",
        L"Blur mode",
        L"Amount of background blur applied to the glass surface.",
        SettingsFieldType::Enum, L"medium",
        {
            {L"none",   L"None"},
            {L"light",  L"Light blur"},
            {L"medium", L"Medium blur"},
            {L"heavy",  L"Heavy blur"},
        },
        30
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"dark_glass.style.corner_radius",
        L"Corner radius (px)",
        L"Rounded corner radius for fence windows (0 = square corners).",
        SettingsFieldType::Int, L"8", {}, 40
    });

    context.settingsRegistry->RegisterPage(std::move(page));

    // Read persisted values and apply the theme
    const bool enabled = (context.settingsRegistry->GetValue(
        L"dark_glass.style.enabled", L"true") == L"true");

    if (enabled)
    {
        // Future: call into an appearance API to set colours, DWM attributes, etc.
        // For now the settings are declared and persisted for use by FenceWindow.
    }

    return true;
}

void DarkGlassThemePlugin::Shutdown()
{
    // Future: restore default fence window colours here.
}
