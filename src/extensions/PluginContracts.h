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
class SpaceExtensionRegistry;

// Simple space metadata for plugin queries
struct SpaceMetadata
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

struct SpaceItemMetadata
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
    SpaceMetadata space;
    std::optional<SpaceItemMetadata> item;
};

struct SpaceCreateRequest
{
    std::wstring title;
    int width = 320;
    int height = 240;
    std::wstring contentType = L"file_collection";
    std::wstring contentPluginId = L"core.file_collection";
    std::wstring contentSource;
};

struct SpacePresentationSettings
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

    virtual std::wstring CreateSpaceNearCursor() = 0;
    virtual std::wstring CreateSpaceNearCursor(const SpaceCreateRequest& request) = 0;
    virtual void ExitApplication() = 0;
    virtual void OpenSettings() = 0;

    // Space query APIs (v0.0.010+)
    // Enables plugins to discover active space and query backing folder paths
    virtual CommandContext GetCurrentCommandContext() const = 0;
    virtual SpaceMetadata GetActiveSpaceMetadata() const = 0;
    virtual std::vector<std::wstring> GetAllSpaceIds() const = 0;
    virtual SpaceMetadata GetSpaceMetadata(const std::wstring& spaceId) const = 0;
    virtual void RefreshSpace(const std::wstring& spaceId) = 0;
    virtual void UpdateSpaceContentSource(const std::wstring& spaceId, const std::wstring& contentSource) = 0;
    virtual void UpdateSpaceContentState(const std::wstring& spaceId,
                                         const std::wstring& state,
                                         const std::wstring& detail) = 0;
    virtual void UpdateSpacePresentation(const std::wstring& spaceId,
                                         const SpacePresentationSettings& settings) = 0;
};

struct PluginManifest
{
    std::wstring id;
    std::wstring displayName;
    std::wstring version;
    std::wstring description;
    int minHostApiVersion = SimpleSpacesVersion::kPluginApiVersion;
    int maxHostApiVersion = SimpleSpacesVersion::kPluginApiVersion;
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
    SpaceExtensionRegistry* spaceExtensionRegistry = nullptr;
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
