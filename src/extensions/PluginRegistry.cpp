#include "extensions/PluginRegistry.h"

void PluginRegistry::Upsert(const PluginStatus& status)
{
    for (auto& existing : m_plugins)
    {
        if (existing.manifest.id == status.manifest.id)
        {
            existing = status;
            return;
        }
    }

    m_plugins.push_back(status);
}

const std::vector<PluginStatus>& PluginRegistry::GetAll() const
{
    return m_plugins;
}
