#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>

/// Manages the one-true source of truth for item ownership
class DesktopItemBroker {
public:
    DesktopItemBroker();
    ~DesktopItemBroker();

    /// Initialize item broker
    bool Initialize();

    /// Move item from desktop into fence backing folder
    bool MoveDesktopItemToFence(const std::wstring& sourcePath, const std::wstring& targetFenceId);

    /// Move item from fence backing folder back to desktop
    bool MoveFenceItemToDesktop(const std::wstring& sourceFenceId, const std::wstring& itemPath);

    /// Move item between fences
    bool MoveFenceItemToFence(const std::wstring& sourceFenceId, const std::wstring& targetFenceId, const std::wstring& itemPath);

    /// Get fence backing folder path
    bool GetFenceBackingFolder(const std::wstring& fenceId, std::wstring& outPath);

    /// Create fence backing folder
    bool CreateFenceBackingFolder(const std::wstring& fenceId, std::wstring& outPath);

private:
    bool MoveFileTransaction(const std::wstring& source, const std::wstring& dest);
    bool HandleNameCollision(const std::wstring& destPath, std::wstring& outResolvedPath);

    std::wstring m_appDataFolder;
    std::wstring m_fencesStorageFolder;
};
