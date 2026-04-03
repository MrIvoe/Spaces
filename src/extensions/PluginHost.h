#pragma once

#include <memory>
#include <vector>

#include "extensions/PluginContracts.h"
#include "extensions/PluginRegistry.h"

class PluginHost
{
public:
    PluginHost();
    ~PluginHost();

    bool LoadBuiltins(const PluginContext& context);
    bool ReloadBuiltins(const PluginContext& context);
    void Shutdown();

    const PluginRegistry& GetRegistry() const;

private:
    std::vector<std::unique_ptr<IPlugin>> m_plugins;
    PluginRegistry m_registry;
};
