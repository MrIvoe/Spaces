#include "extensions/PluginHost.h"

#include "core/Diagnostics.h"
#include "plugins/builtins/BuiltinPlugins.h"

PluginHost::PluginHost() = default;

PluginHost::~PluginHost()
{
    Shutdown();
}

bool PluginHost::LoadBuiltins(const PluginContext& context)
{
    m_plugins = CreateBuiltinPlugins();

    bool allLoaded = true;
    for (auto& plugin : m_plugins)
    {
        PluginStatus status;
        status.manifest = plugin->GetManifest();
        status.enabled = status.manifest.enabledByDefault;

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
        catch (...)
        {
            status.loaded = false;
            status.lastError = L"Initialize threw an exception.";
            allLoaded = false;
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
                    L"Plugin loaded: id='" + status.manifest.id + L"' capabilities='" + capabilities + L"'");
            }
            else
            {
                context.diagnostics->Error(
                    L"Plugin failed: id='" + status.manifest.id + L"' error='" + status.lastError + L"'");
            }
        }
    }

    return allLoaded;
}

void PluginHost::Shutdown()
{
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
    {
        if (*it)
        {
            (*it)->Shutdown();
        }
    }

    m_plugins.clear();
}

const PluginRegistry& PluginHost::GetRegistry() const
{
    return m_registry;
}
