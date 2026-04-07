#include "extensions/PluginHost.h"

#include "AppVersion.h"
#include "core/Diagnostics.h"
#include "core/CommandDispatcher.h"
#include "core/PluginAppearanceConflictGuard.h"
#include "extensions/PluginSettingsRegistry.h"
#include "plugins/builtins/BuiltinPlugins.h"

#include <algorithm>
#include <set>

#include <windows.h>

namespace
{
    std::wstring Utf8ToWString(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0)
        {
            return L"(message conversion failed)";
        }

        std::wstring wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }

    bool EvaluateManifestCompatibility(const PluginManifest& manifest, std::wstring& reason)
    {
        if (manifest.id.empty())
        {
            reason = L"Manifest id is empty.";
            return false;
        }

        if (manifest.displayName.empty())
        {
            reason = L"Manifest displayName is empty.";
            return false;
        }

        if (manifest.version.empty())
        {
            reason = L"Manifest version is empty.";
            return false;
        }

        if (manifest.minHostApiVersion > manifest.maxHostApiVersion)
        {
            reason = L"Manifest minHostApiVersion is greater than maxHostApiVersion.";
            return false;
        }

        const int hostApi = SimpleFencesVersion::kPluginApiVersion;
        if (hostApi < manifest.minHostApiVersion || hostApi > manifest.maxHostApiVersion)
        {
            reason = L"Plugin API range is incompatible with host API version " + std::to_wstring(hostApi) + L".";
            return false;
        }

        return true;
    }
}

PluginHost::PluginHost() = default;

PluginHost::~PluginHost()
{
    Shutdown();
}

bool PluginHost::LoadBuiltins(const PluginContext& context)
{
    m_commandDispatcher = context.commandDispatcher;
    m_registry.Clear();
    m_plugins.clear();
    m_registeredPluginCommands.clear();

    try
    {
        m_plugins = CreateBuiltinPlugins();
    }
    catch (const std::exception& ex)
    {
        if (context.diagnostics)
        {
            context.diagnostics->Error(L"Plugin creation failed: " + Utf8ToWString(ex.what()));
        }
        return false;
    }
    catch (...)
    {
        if (context.diagnostics)
        {
            context.diagnostics->Error(L"Plugin creation failed with unknown exception.");
        }
        return false;
    }

    bool allLoaded = true;
    std::set<std::wstring> seenPluginIds;
    for (auto& plugin : m_plugins)
    {
        PluginStatus status;
        try
        {
            status.manifest = plugin->GetManifest();
        }
        catch (const std::exception& ex)
        {
            status.enabled = false;
            status.loaded = false;
            status.lastError = L"GetManifest threw exception: " + Utf8ToWString(ex.what());
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(status.lastError);
            }
            continue;
        }
        catch (...)
        {
            status.enabled = false;
            status.loaded = false;
            status.lastError = L"GetManifest threw unknown exception.";
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(status.lastError);
            }
            continue;
        }

        std::wstring manifestCompatibilityReason;
        if (!EvaluateManifestCompatibility(status.manifest, manifestCompatibilityReason))
        {
            status.enabled = false;
            status.loaded = false;
            status.compatibilityStatus = L"incompatible";
            status.compatibilityReason = manifestCompatibilityReason;
            status.lastError = manifestCompatibilityReason;
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(L"Plugin manifest rejected: id='" + status.manifest.id + L"' reason='" + manifestCompatibilityReason + L"'");
            }
            continue;
        }
        status.compatibilityStatus = L"compatible";
        status.compatibilityReason = L"Host API range satisfied.";

        if (seenPluginIds.find(status.manifest.id) != seenPluginIds.end())
        {
            status.enabled = false;
            status.loaded = false;
            status.compatibilityStatus = L"rejected";
            status.compatibilityReason = L"Duplicate plugin id detected.";
            status.lastError = L"Duplicate plugin id detected.";
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(L"Plugin rejected due to duplicate id: " + status.manifest.id);
            }
            continue;
        }
        seenPluginIds.insert(status.manifest.id);

        status.enabled = status.manifest.enabledByDefault;
        if (context.settingsRegistry)
        {
            const std::wstring overrideKey = L"settings.plugins.enable." + status.manifest.id;
            const std::wstring overrideValue = context.settingsRegistry->GetValue(overrideKey, L"");
            if (overrideValue == L"true")
            {
                status.enabled = true;
            }
            else if (overrideValue == L"false")
            {
                status.enabled = false;
            }
        }

        if (!status.enabled)
        {
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Info(L"Plugin disabled by default: " + status.manifest.id);
            }
            continue;
        }

        bool loaded = false;
        std::vector<std::wstring> commandIdsBeforeInit;
        if (context.commandDispatcher)
        {
            commandIdsBeforeInit = context.commandDispatcher->ListCommandIds();
        }

        try
        {
            loaded = plugin->Initialize(context);
            status.loaded = loaded;
            if (!loaded)
            {
                status.lastError = L"Initialize returned false.";
                allLoaded = false;
            }
        }
        catch (const std::exception& ex)
        {
            status.loaded = false;
            status.lastError = L"Initialize threw exception: " + Utf8ToWString(ex.what());
            allLoaded = false;
        }
        catch (...)
        {
            status.loaded = false;
            status.lastError = L"Initialize threw unknown exception.";
            allLoaded = false;
        }

        if (loaded && context.commandDispatcher)
        {
            const auto commandIdsAfterInit = context.commandDispatcher->ListCommandIds();

            std::vector<std::wstring> pluginCommandIds;
            pluginCommandIds.reserve(commandIdsAfterInit.size());
            for (const auto& commandId : commandIdsAfterInit)
            {
                if (std::find(commandIdsBeforeInit.begin(), commandIdsBeforeInit.end(), commandId) == commandIdsBeforeInit.end())
                {
                    pluginCommandIds.push_back(commandId);
                }
            }

            PluginAppearanceConflictGuard conflictGuard;
            if (conflictGuard.HasAppearanceConflict(status.manifest.id, pluginCommandIds))
            {
                for (const auto& commandId : pluginCommandIds)
                {
                    context.commandDispatcher->UnregisterCommand(commandId);
                }

                status.loaded = false;
                status.lastError = L"Appearance command path disabled due to selector ownership conflict.";
                allLoaded = false;

                if (context.diagnostics)
                {
                    context.diagnostics->Warn(
                        L"Plugin appearance command path disabled: id='" + status.manifest.id +
                        L"' owner='" + PluginAppearanceConflictGuard::GetCanonicalAppearanceSelectorId() + L"'");
                }
            }
            else
            {
                m_registeredPluginCommands[status.manifest.id] = pluginCommandIds;
            }
        }

        m_registry.Upsert(status);

        if (context.diagnostics)
        {
            std::wstring capabilities;
            for (size_t i = 0; i < status.manifest.capabilities.size(); ++i)
            {
                capabilities += status.manifest.capabilities[i];
                if (i + 1 < status.manifest.capabilities.size())
                {
                    capabilities += L", ";
                }
            }

            if (status.loaded)
            {
                context.diagnostics->Info(
                    L"Plugin loaded: id='" + status.manifest.id +
                    L"' compatibility='" + status.compatibilityStatus +
                    L"' capabilities='" + capabilities + L"'");
            }
            else
            {
                context.diagnostics->Error(
                    L"Plugin failed: id='" + status.manifest.id +
                    L"' compatibility='" + status.compatibilityStatus +
                    L"' reason='" + status.compatibilityReason +
                    L"' error='" + status.lastError + L"'");
            }
        }
    }

    return allLoaded;
}

bool PluginHost::ReloadBuiltins(const PluginContext& context)
{
    Shutdown();
    return LoadBuiltins(context);
}

void PluginHost::Shutdown()
{
    if (m_commandDispatcher)
    {
        for (const auto& entry : m_registeredPluginCommands)
        {
            for (const auto& commandId : entry.second)
            {
                m_commandDispatcher->UnregisterCommand(commandId);
            }
        }
    }
    m_registeredPluginCommands.clear();

    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
    {
        if (*it)
        {
            try
            {
                (*it)->Shutdown();
            }
            catch (...)
            {
                // Keep shutdown resilient even if a plugin misbehaves.
            }
        }
    }

    m_plugins.clear();
    m_commandDispatcher = nullptr;
}

const PluginRegistry& PluginHost::GetRegistry() const
{
    return m_registry;
}

