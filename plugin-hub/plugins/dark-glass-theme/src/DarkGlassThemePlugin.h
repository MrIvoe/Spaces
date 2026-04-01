#pragma once

#include "extensions/PluginContracts.h"

// Community plugin: Dark Glass Theme
// Registers appearance settings for a translucent dark fence style.
// Capability: appearance, settings_pages
class DarkGlassThemePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override;
    bool Initialize(const PluginContext& context) override;
    void Shutdown() override;
};
