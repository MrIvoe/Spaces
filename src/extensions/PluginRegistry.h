#pragma once

#include <string>
#include <vector>

#include "extensions/PluginContracts.h"

struct PluginStatus
{
    PluginManifest manifest;
    bool enabled = false;
    bool loaded = false;
    std::wstring lastError;
};

class PluginRegistry
{
public:
    void Upsert(const PluginStatus& status);
    const std::vector<PluginStatus>& GetAll() const;

private:
    std::vector<PluginStatus> m_plugins;
};
