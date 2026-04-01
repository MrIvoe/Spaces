#include "ClockWidgetPlugin.h"

#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"

PluginManifest ClockWidgetPlugin::GetManifest() const
{
    PluginManifest m;
    m.id           = L"community.clock_widget";
    m.displayName  = L"Clock Widget";
    m.version      = L"1.0.0";
    m.description  = L"Live digital or analogue clock inside a fence widget panel.";
    m.capabilities = {L"widgets", L"settings_pages"};
    return m;
}

bool ClockWidgetPlugin::Initialize(const PluginContext& context)
{
    if (!context.settingsRegistry)
    {
        return true;
    }

    // --- Clock appearance page ---
    PluginSettingsPage page;
    page.pluginId = L"community.clock_widget";
    page.pageId   = L"clock.display";
    page.title    = L"Clock Display";
    page.order    = 10;

    page.fields.push_back(SettingsFieldDescriptor{
        L"clock.display.style",
        L"Clock style",
        L"Analogue hand clock or digital text readout.",
        SettingsFieldType::Enum, L"digital",
        {
            {L"digital",  L"Digital"},
            {L"analogue", L"Analogue"},
        },
        10
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"clock.display.show_seconds",
        L"Show seconds",
        L"Include the seconds hand / digit in the clock display.",
        SettingsFieldType::Bool, L"true", {}, 20
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"clock.display.use_24h",
        L"24-hour format",
        L"Use 24-hour (military) time format instead of 12-hour AM/PM.",
        SettingsFieldType::Bool, L"false", {}, 30
    });

    page.fields.push_back(SettingsFieldDescriptor{
        L"clock.display.timezone",
        L"Timezone override",
        L"IANA timezone name (e.g. America/New_York). Leave blank to use the system timezone.",
        SettingsFieldType::String, L"", {}, 40
    });

    context.settingsRegistry->RegisterPage(std::move(page));

    return true;
}

void ClockWidgetPlugin::Shutdown() {}
