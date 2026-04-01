#pragma once

#include "extensions/SettingsSchema.h"

#include <string>
#include <vector>

struct PluginStatusView
{
    std::wstring id;
    std::wstring displayName;
    std::wstring version;
    bool enabled = false;
    bool loaded = false;
    std::wstring lastError;
    std::vector<std::wstring> capabilities;
};

struct SettingsPageView
{
    std::wstring pluginId;
    std::wstring pageId;
    std::wstring title;
    int order = 0;
    std::vector<SettingsFieldDescriptor> fields; // interactive controls declared by the plugin
};
