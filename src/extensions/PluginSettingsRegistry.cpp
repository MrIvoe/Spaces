#include "extensions/PluginSettingsRegistry.h"

#include "core/SettingsStore.h"

#include <algorithm>
#include <unordered_map>

void PluginSettingsRegistry::SetStore(SettingsStore* store)
{
    m_store = store;
}

void PluginSettingsRegistry::RegisterPage(const PluginSettingsPage& page)
{
    m_pages.push_back(page);
}

std::vector<PluginSettingsPage> PluginSettingsRegistry::GetAllPages() const
{
    std::vector<PluginSettingsPage> pages = m_pages;
    std::sort(pages.begin(), pages.end(), [](const PluginSettingsPage& a, const PluginSettingsPage& b) {
        if (a.order == b.order)
        {
            return a.title < b.title;
        }
        return a.order < b.order;
    });

    return pages;
}

std::wstring PluginSettingsRegistry::GetValue(const std::wstring& key, const std::wstring& defaultValue) const
{
    if (m_store)
    {
        return m_store->Get(key, defaultValue);
    }
    const auto it = m_memValues.find(key);
    return (it != m_memValues.end()) ? it->second : defaultValue;
}

void PluginSettingsRegistry::SetValue(const std::wstring& key, const std::wstring& value)
{
    if (m_store)
    {
        m_store->Set(key, value);
        return;
    }
    m_memValues[key] = value;
}
