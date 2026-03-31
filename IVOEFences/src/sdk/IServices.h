#pragma once

#include <cstdint>
#include <windows.h>
#include <string>

/// Describes a fence in the system
struct FenceModel {
    std::wstring id;                        // Unique fence ID
    std::wstring title;                     // Display title
    RECT rect;                              // Position and size
    bool rolledUp;                          // Currently collapsed
    float opacity;                          // 0.0 - 1.0, default 0.7
    std::wstring backingFolder;             // Where items are physically stored
    std::wstring viewProviderId;            // e.g. "core.items" or "plugin.explorer.view"
};

/// Service for querying and modifying fences
struct IFenceService {
    virtual ~IFenceService() = default;

    /// Create a new fence
    virtual bool CreateFence(const wchar_t* title, const RECT* rect, const wchar_t* viewProviderId, std::wstring& outFenceId) = 0;

    /// Delete an existing fence
    virtual bool DeleteFence(const wchar_t* fenceId) = 0;

    /// Get fence by ID
    virtual bool GetFence(const wchar_t* fenceId, FenceModel& outFence) = 0;

    /// Update fence properties
    virtual bool UpdateFence(const wchar_t* fenceId, const FenceModel& fence) = 0;

    /// Find fence at screen point
    virtual const wchar_t* FindFenceAtPoint(int screenX, int screenY) = 0;

    /// Get HWND of fence window
    virtual HWND GetFenceHwnd(const wchar_t* fenceId) = 0;

    /// Set fence rolled-up state
    virtual bool SetFenceRolledUp(const wchar_t* fenceId, bool rolledUp) = 0;

    /// Set fence opacity
    virtual bool SetFenceOpacity(const wchar_t* fenceId, float opacity) = 0;
};

/// Service for desktop items
struct IDesktopItemService {
    virtual ~IDesktopItemService() = default;

    /// Get list of items currently on desktop
    virtual uint32_t EnumerateDesktopItems(void* outArray) = 0;

    /// Get list of items in a fence
    virtual uint32_t EnumerateFenceItems(const wchar_t* fenceId, void* outArray) = 0;
};

/// Service for shell icons
struct IIconService {
    virtual ~IIconService() = default;

    /// Get system image list handle
    virtual HIMAGELIST GetSystemImageList() = 0;

    /// Get icon index for file path
    virtual int GetIconIndex(const wchar_t* filePath) = 0;
};

/// Service for window utilities
struct IWindowService {
    virtual ~IWindowService() = default;

    /// Get desktop window HWND
    virtual HWND GetDesktopWindow() = 0;

    /// Get WorkerW window HWND
    virtual HWND GetWorkerW() = 0;
};

/// Service for settings storage
struct ISettingsStore {
    virtual ~ISettingsStore() = default;

    /// Read string setting
    virtual bool ReadString(const wchar_t* key, std::wstring& outValue) = 0;

    /// Write string setting
    virtual bool WriteString(const wchar_t* key, const wchar_t* value) = 0;

    /// Read integer setting
    virtual bool ReadInt(const wchar_t* key, int32_t& outValue) = 0;

    /// Write integer setting
    virtual bool WriteInt(const wchar_t* key, int32_t value) = 0;

    /// Read float setting
    virtual bool ReadFloat(const wchar_t* key, float& outValue) = 0;

    /// Write float setting
    virtual bool WriteFloat(const wchar_t* key, float value) = 0;

    /// Delete setting
    virtual bool DeleteKey(const wchar_t* key) = 0;
};

/// Service for logging
struct ILogger {
    virtual ~ILogger() = default;

    virtual void Debug(const wchar_t* message) = 0;
    virtual void Info(const wchar_t* message) = 0;
    virtual void Warning(const wchar_t* message) = 0;
    virtual void Error(const wchar_t* message) = 0;
};
