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

    /// Move item from desktop into space backing folder
    bool MoveDesktopItemToSpace(const std::wstring& sourcePath, const std::wstring& targetSpaceId);

    /// Move item from space backing folder back to desktop
    bool MoveSpaceItemToDesktop(const std::wstring& sourceSpaceId, const std::wstring& itemPath);

    /// Move item between spaces
    bool MoveSpaceItemToSpace(const std::wstring& sourceSpaceId, const std::wstring& targetSpaceId, const std::wstring& itemPath);

    /// Get space backing folder path
    bool GetSpaceBackingFolder(const std::wstring& spaceId, std::wstring& outPath);

    /// Create space backing folder
    bool CreateSpaceBackingFolder(const std::wstring& spaceId, std::wstring& outPath);

private:
    bool MoveFileTransaction(const std::wstring& source, const std::wstring& dest);
    bool HandleNameCollision(const std::wstring& destPath, std::wstring& outResolvedPath);

    std::wstring m_appDataFolder;
    std::wstring m_spacesStorageFolder;
};
