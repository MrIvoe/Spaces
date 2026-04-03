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

void PluginRegistry::Clear()
{
    m_plugins.clear();
}

const std::vector<PluginStatus>& PluginRegistry::GetAll() const
{
    return m_plugins;
}

const PluginStatus* PluginRegistry::FindById(const std::wstring& id) const
{
    for (const auto& plugin : m_plugins)
    {
        if (plugin.manifest.id == id)
        {
            return &plugin;
        }
    }

    return nullptr;
}
