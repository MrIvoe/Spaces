#pragma once

#include <cstdint>
#include <windows.h>

/// Host API interface - exposed by core to plugins
struct IHostApi {
    virtual ~IHostApi() = default;

    /// Get API version for compatibility checking
    virtual uint32_t GetApiVersion() const = 0;

    /// Register a command that can be invoked via menu or shortcut
    virtual void RegisterCommand(const wchar_t* commandId, const wchar_t* displayName, void* handler) = 0;

    /// Register a global settings page provider
    virtual void RegisterSettingsPage(const wchar_t* pageId, const wchar_t* pageTitle, void* provider) = 0;

    /// Register a per-fence settings provider
    virtual void RegisterFenceSettingsPage(const wchar_t* pageId, const wchar_t* pageTitle, void* provider) = 0;

    /// Register a fence view content provider (for custom fence types)
    virtual void RegisterFenceViewProvider(const wchar_t* providerId, const wchar_t* displayName, void* provider) = 0;

    /// Register a render hook for custom fence drawing
    virtual void RegisterRenderHook(const wchar_t* hookId, void* hook) = 0;

    /// Get settings store for reading/writing plugin settings
    virtual void* GetSettingsStore() = 0;

    /// Get fence service for querying/modifying fences
    virtual void* GetFenceService() = 0;

    /// Get desktop item service for querying desktop items
    virtual void* GetDesktopItemService() = 0;

    /// Get icon service for image list and icon access
    virtual void* GetIconService() = 0;

    /// Get window service for window utilities
    virtual void* GetWindowService() = 0;

    /// Get logger for debug/error output
    virtual void* GetLogger() = 0;
};
