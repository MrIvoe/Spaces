#pragma once

#include <string>

struct PluginHubSyncResult
{
    bool success = false;
    int installedPluginCount = 0;
    std::wstring message;
};

namespace PluginHubSync
{
    // Sync plugin-hub repository and mirror its plugins/* folders into
    // %LOCALAPPDATA%\SimpleFences\plugins.
    PluginHubSyncResult SyncFromRepository(const std::wstring& repoUrl, const std::wstring& branch);
}
