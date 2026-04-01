#pragma once

#include <memory>
#include <string>
#include <vector>

class CommandDispatcher;
class EventBus;
class Diagnostics;
class PluginSettingsRegistry;
class MenuContributionRegistry;
class FenceExtensionRegistry;

class IApplicationCommands
{
public:
    virtual ~IApplicationCommands() = default;

    virtual void CreateFenceNearCursor() = 0;
    virtual void ExitApplication() = 0;
    virtual void OpenSettings() = 0;
};

struct PluginManifest
{
    std::wstring id;
    std::wstring displayName;
    std::wstring version;
    std::wstring description;
    bool enabledByDefault = true;
    std::vector<std::wstring> capabilities;
};

struct PluginContext
{
    CommandDispatcher* commandDispatcher = nullptr;
    EventBus* eventBus = nullptr;
    Diagnostics* diagnostics = nullptr;
    PluginSettingsRegistry* settingsRegistry = nullptr;
    MenuContributionRegistry* menuRegistry = nullptr;
    FenceExtensionRegistry* fenceExtensionRegistry = nullptr;
    IApplicationCommands* appCommands = nullptr;
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;

    virtual PluginManifest GetManifest() const = 0;
    virtual bool Initialize(const PluginContext& context) = 0;
    virtual void Shutdown() = 0;
};
