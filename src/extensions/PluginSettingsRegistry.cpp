#include "extensions/PluginSettingsRegistry.h"

#include "core/SettingsStore.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    bool IsValidField(const SettingsFieldDescriptor& field)
    {
        if (field.key.empty() || field.label.empty())
        {
            return false;
        }

        if (field.type == SettingsFieldType::Enum && field.options.empty())
        {
            return false;
        }

        return true;
    }

    bool EnumContainsValue(const SettingsFieldDescriptor& field, const std::wstring& value)
    {
        for (const auto& option : field.options)
        {
            if (option.value == value)
            {
                return true;
            }
        }

        return false;
    }
}

void PluginSettingsRegistry::SetStore(SettingsStore* store)
{
    m_store = store;
}

bool PluginSettingsRegistry::RegisterPage(const PluginSettingsPage& page)
{
    if (page.pluginId.empty() || page.pageId.empty() || page.title.empty())
    {
        return false;
    }

    PluginSettingsPage normalized = page;
    normalized.fields.erase(
        std::remove_if(normalized.fields.begin(), normalized.fields.end(), [](const SettingsFieldDescriptor& field) {
            return !IsValidField(field);
        }),
        normalized.fields.end());

    for (auto& field : normalized.fields)
    {
        if (field.type == SettingsFieldType::Enum && !EnumContainsValue(field, field.defaultValue))
        {
            field.defaultValue = field.options.front().value;
        }
    }

    std::sort(normalized.fields.begin(), normalized.fields.end(), [](const SettingsFieldDescriptor& a, const SettingsFieldDescriptor& b) {
        if (a.order == b.order)
        {
            return a.label < b.label;
        }
        return a.order < b.order;
    });

    for (auto& existing : m_pages)
    {
        if (existing.pluginId == normalized.pluginId && existing.pageId == normalized.pageId)
        {
            existing = std::move(normalized);
            return true;
        }
    }

    m_pages.push_back(std::move(normalized));
    return true;
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

void PluginSettingsRegistry::ClearPages()
{
    m_pages.clear();
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
    }
    else
    {
        m_memValues[key] = value;
    }

    for (const auto& [token, observer] : m_observers)
    {
        (void)token;
        if (observer)
        {
            observer(key, value);
        }
    }
}

int PluginSettingsRegistry::RegisterObserver(SettingsObserver observer)
{
    const int token = m_nextObserverToken++;
    m_observers[token] = std::move(observer);
    return token;
}

void PluginSettingsRegistry::UnregisterObserver(int observerToken)
{
    m_observers.erase(observerToken);
}
