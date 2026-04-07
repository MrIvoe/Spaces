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

    if (!dispatcher.HasCommand(L"appearance.mode.focus") ||
        !dispatcher.HasCommand(L"appearance.mode.gallery") ||
        !dispatcher.HasCommand(L"appearance.mode.quiet"))
    {
        host.Shutdown();
        return Fail("Plugin host runtime test: appearance selector commands should remain registered");
    }

    host.Shutdown();
    return 0;
}
