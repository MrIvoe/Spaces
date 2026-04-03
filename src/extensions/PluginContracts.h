#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AppVersion.h"

class CommandDispatcher;
class EventBus;
class Diagnostics;
class PluginSettingsRegistry;
class MenuContributionRegistry;
class FenceExtensionRegistry;

// Simple fence metadata for plugin queries
struct FenceMetadata
{
    std::wstring id;
    std::wstring title;
    std::wstring backingFolderPath;
    std::wstring contentType;
    std::wstring contentPluginId;
    std::wstring contentSource;
    std::wstring contentState;
    std::wstring contentStateDetail;
};

struct FenceItemMetadata
{
    std::wstring name;
    std::wstring fullPath;
    std::wstring originalPath;
    bool isDirectory = false;
};

struct CommandContext
{
    std::wstring commandId;
    std::wstring invocationSource;
    FenceMetadata fence;
    std::optional<FenceItemMetadata> item;
};

struct FenceCreateRequest
{
    std::wstring title;
    std::wstring contentType = L"file_collection";
    std::wstring contentPluginId = L"core.file_collection";
    std::wstring contentSource;
};

struct FencePresentationSettings
{
    std::optional<bool> textOnlyMode;
    std::optional<bool> rollupWhenNotHovered;
    std::optional<bool> transparentWhenNotHovered;
    std::optional<bool> labelsOnHover;
    std::optional<std::wstring> iconSpacingPreset;
    bool applyToAll = false;
};

class IApplicationCommands
{
public:
    virtual ~IApplicationCommands() = default;

    virtual std::wstring CreateFenceNearCursor() = 0;
    virtual std::wstring CreateFenceNearCursor(const FenceCreateRequest& request) = 0;
    virtual void ExitApplication() = 0;
    virtual void OpenSettings() = 0;

    // Fence query APIs (v0.0.010+)
    // Enables plugins to discover active fence and query backing folder paths
    virtual CommandContext GetCurrentCommandContext() const = 0;
    virtual FenceMetadata GetActiveFenceMetadata() const = 0;
    virtual std::vector<std::wstring> GetAllFenceIds() const = 0;
    virtual FenceMetadata GetFenceMetadata(const std::wstring& fenceId) const = 0;
    virtual void RefreshFence(const std::wstring& fenceId) = 0;
    virtual void UpdateFenceContentSource(const std::wstring& fenceId, const std::wstring& contentSource) = 0;
    virtual void UpdateFenceContentState(const std::wstring& fenceId,
                                         const std::wstring& state,
                                         const std::wstring& detail) = 0;
    virtual void UpdateFencePresentation(const std::wstring& fenceId,
                                         const FencePresentationSettings& settings) = 0;
};

struct PluginManifest
{
    std::wstring id;
    std::wstring displayName;
    std::wstring version;
    std::wstring description;
    int minHostApiVersion = SimpleFencesVersion::kPluginApiVersion;
    int maxHostApiVersion = SimpleFencesVersion::kPluginApiVersion;
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
