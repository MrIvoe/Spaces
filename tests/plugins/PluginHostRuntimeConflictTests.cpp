#include <string>
#include <iostream>

#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "extensions/FenceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginHost.h"
#include "extensions/PluginSettingsRegistry.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }
}

int RunPluginHostRuntimeConflictTests()
{
    {
        CommandDispatcher dispatcher;
        Diagnostics diagnostics;
        PluginSettingsRegistry settingsRegistry;
        MenuContributionRegistry menuRegistry;
        FenceExtensionRegistry fenceExtensionRegistry;

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.fenceExtensionRegistry = &fenceExtensionRegistry;

        PluginHost host;
        host.LoadBuiltins(context);

        const auto* appearance = host.GetRegistry().FindById(L"community.visual_modes");
        if (!appearance)
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: community.visual_modes should be present in registry");
        }

        if (!appearance->loaded)
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: community.visual_modes should remain loaded under selector ownership enforcement");
        }

        if (appearance->compatibilityStatus != L"compatible")
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: compatibility status should be surfaced as compatible for community.visual_modes");
        }

        if (!dispatcher.HasCommand(L"appearance.mode.focus") ||
            !dispatcher.HasCommand(L"appearance.mode.gallery") ||
            !dispatcher.HasCommand(L"appearance.mode.quiet"))
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: appearance selector commands should remain registered");
        }

        host.Shutdown();
    }

    {
        CommandDispatcher dispatcher;
        Diagnostics diagnostics;
        PluginSettingsRegistry settingsRegistry;
        MenuContributionRegistry menuRegistry;
        FenceExtensionRegistry fenceExtensionRegistry;

        settingsRegistry.SetValue(L"settings.plugins.enable.community.visual_modes", L"false");

        PluginContext context;
        context.commandDispatcher = &dispatcher;
        context.diagnostics = &diagnostics;
        context.settingsRegistry = &settingsRegistry;
        context.menuRegistry = &menuRegistry;
        context.fenceExtensionRegistry = &fenceExtensionRegistry;

        PluginHost host;
        host.LoadBuiltins(context);

        const auto* appearance = host.GetRegistry().FindById(L"community.visual_modes");
        if (!appearance)
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: community.visual_modes should still be present in registry when overridden disabled");
        }

        if (appearance->enabled)
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: community.visual_modes should be disabled by persisted override");
        }

        if (appearance->loaded)
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: community.visual_modes should not be loaded when override forces disabled state");
        }

        if (dispatcher.HasCommand(L"appearance.mode.focus") ||
            dispatcher.HasCommand(L"appearance.mode.gallery") ||
            dispatcher.HasCommand(L"appearance.mode.quiet"))
        {
            host.Shutdown();
            return Fail("Plugin host runtime test: appearance selector commands should not be registered when plugin is override-disabled");
        }

        host.Shutdown();
    }

    return 0;
}
