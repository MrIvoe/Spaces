#pragma once

#include "extensions/PluginContracts.h"

// Community plugin: Network Drive Fence
// Registers a fence_content_provider that browses a UNC / mapped drive path.
class NetworkDriveFencePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override;
    bool Initialize(const PluginContext& context) override;
    void Shutdown() override;
};
