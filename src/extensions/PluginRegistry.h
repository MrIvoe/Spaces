#pragma once

#include <string>
#include <vector>

#include "extensions/PluginContracts.h"

struct PluginStatus
{
    PluginManifest manifest;
    bool enabled = false;
    bool loaded = false;
    std::wstring compatibilityStatus = L"unknown";
    std::wstring compatibilityReason;
    std::wstring lastError;
};

class PluginRegistry
{
public:
    void Upsert(const PluginStatus& status);
    void Clear();
    const std::vector<PluginStatus>& GetAll() const;
    const PluginStatus* FindById(const std::wstring& id) const;

private:
    std::vector<PluginStatus> m_plugins;
};
