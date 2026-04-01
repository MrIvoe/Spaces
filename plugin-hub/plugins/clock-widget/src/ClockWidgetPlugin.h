#pragma once

#include "extensions/PluginContracts.h"

// Community plugin: Clock Widget
// Renders a live clock inside a fence widget panel.
class ClockWidgetPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override;
    bool Initialize(const PluginContext& context) override;
    void Shutdown() override;
};
