#include "extensions/PluginSettingsRegistry.h"

#include <algorithm>

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
