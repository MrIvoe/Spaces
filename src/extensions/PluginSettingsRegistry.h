#pragma once

#include <string>
#include <vector>

struct PluginSettingsPage
{
    std::wstring pluginId;
    std::wstring pageId;
    std::wstring title;
    int order = 0;
};

class PluginSettingsRegistry
{
public:
    void RegisterPage(const PluginSettingsPage& page);
    std::vector<PluginSettingsPage> GetAllPages() const;

private:
    std::vector<PluginSettingsPage> m_pages;
};
