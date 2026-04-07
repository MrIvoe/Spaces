#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "core/CommandDispatcher.h"
#include "core/PluginAppearanceConflictGuard.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return 1;
    }

    struct LoaderConflictResult
    {
        bool conflict = false;
        std::wstring log;
        std::vector<std::wstring> registeredCommands;
    };

    LoaderConflictResult RunLoaderConflictPass(
        CommandDispatcher& dispatcher,
        const std::wstring& pluginId,
        const std::vector<std::wstring>& commandsToRegister)
    {
        const auto before = dispatcher.ListCommandIds();

        for (const auto& commandId : commandsToRegister)
        {
            dispatcher.RegisterCommand(commandId, []() {});
        }

        const auto after = dispatcher.ListCommandIds();

        std::vector<std::wstring> added;
        for (const auto& id : after)
        {
            if (std::find(before.begin(), before.end(), id) == before.end())
            {
                added.push_back(id);
            }
        }

        std::wostringstream captured;
        std::wstreambuf* previous = std::wcerr.rdbuf(captured.rdbuf());

        PluginAppearanceConflictGuard guard;
        const bool conflict = guard.HasAppearanceConflict(pluginId, added);

        std::wcerr.rdbuf(previous);

        if (conflict)
        {
            for (const auto& id : added)
            {
                dispatcher.UnregisterCommand(id);
            }
        }

        LoaderConflictResult result;
        result.conflict = conflict;
        result.log = captured.str();
        result.registeredCommands = dispatcher.ListCommandIds();
        return result;
    }

    bool HasCommand(const std::vector<std::wstring>& commands, const std::wstring& command)
    {
        return std::find(commands.begin(), commands.end(), command) != commands.end();
    }
}

int RunPluginLoaderConflictIntegrationTests()
{
    // Loader integration: canonical selector remains enabled.
    {
        CommandDispatcher dispatcher;
        const auto result = RunLoaderConflictPass(
            dispatcher,
            L"community.visual_modes",
            {L"appearance.mode.focus"});

        if (result.conflict)
        {
            return Fail("Plugin loader conflict test 1: canonical selector should not conflict");
        }

        if (!HasCommand(result.registeredCommands, L"appearance.mode.focus"))
        {
            return Fail("Plugin loader conflict test 1: canonical command should remain registered");
        }
    }

    // Loader integration: non-canonical appearance plugin is logged and disabled.
    {
        CommandDispatcher dispatcher;
        const auto result = RunLoaderConflictPass(
            dispatcher,
            L"community.theme_switcher_alt",
            {L"appearance.mode.dark", L"theme.apply"});

        if (!result.conflict)
        {
            return Fail("Plugin loader conflict test 2: non-canonical appearance selector should conflict");
        }

        if (result.log.find(L"community.theme_switcher_alt") == std::wstring::npos ||
            result.log.find(L"community.visual_modes") == std::wstring::npos)
        {
            return Fail("Plugin loader conflict test 2: conflict log should include plugin and canonical selector IDs");
        }

        if (HasCommand(result.registeredCommands, L"appearance.mode.dark") ||
            HasCommand(result.registeredCommands, L"theme.apply"))
        {
            return Fail("Plugin loader conflict test 2: conflicting commands should be disabled by loader path");
        }

        const auto dispatch = dispatcher.DispatchDetailed(L"appearance.mode.dark");
        if (dispatch.handled)
        {
            return Fail("Plugin loader conflict test 2: disabled command should not dispatch");
        }
    }

    // Loader integration: non-appearance command sets are not blocked.
    {
        CommandDispatcher dispatcher;
        const auto result = RunLoaderConflictPass(
            dispatcher,
            L"community.widgets_plus",
            {L"widgets.refresh", L"widget.pin"});

        if (result.conflict)
        {
            return Fail("Plugin loader conflict test 3: non-appearance commands should not conflict");
        }

        if (!HasCommand(result.registeredCommands, L"widgets.refresh"))
        {
            return Fail("Plugin loader conflict test 3: non-appearance command should remain registered");
        }
    }

    return 0;
}
