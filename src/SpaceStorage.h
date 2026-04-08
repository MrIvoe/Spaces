#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include "Models.h"

class SpaceStorage
{
public:
    SpaceStorage();
    ~SpaceStorage() = default;

    std::wstring GetAppRoot() const;
    std::wstring GetSpacesRoot() const;
    std::wstring GetMetadataPath() const;

    std::wstring EnsureSpaceFolder(const std::wstring& spaceId);
    std::vector<SpaceItem> ScanSpaceItems(const std::wstring& folder) const;
    FileMoveResult MovePathsIntoSpace(const std::vector<std::wstring>& sourcePaths, const std::wstring& spaceFolder);
    bool RestoreItemToOrigin(const std::wstring& spaceFolder,
                             const SpaceItem& item,
                             std::wstring* failureReason = nullptr,
                             std::filesystem::path* restoredDestination = nullptr);
    RestoreResult RestoreAllItems(const std::wstring& spaceFolder);
    bool DeleteItem(const std::wstring& spaceFolder, const SpaceItem& item);
    bool DeleteSpaceFolderIfEmpty(const std::wstring& spaceFolder);
    bool MarkSpaceDeleted(const std::wstring& spaceFolder);
    bool IsSpaceDeletedMarked(const std::wstring& spaceFolder) const;
    bool ClearSpaceDeletedMarker(const std::wstring& spaceFolder);

    // Icon utilities
    static int GetFileIconIndex(const std::wstring& filePath);

private:
    std::filesystem::path GenerateNonConflictingPath(const std::filesystem::path& target) const;
    void SaveItemOrigins(const std::wstring& spaceFolder, const std::map<std::wstring, std::wstring>& origins);
    std::map<std::wstring, std::wstring> LoadItemOrigins(const std::wstring& spaceFolder) const;
    std::wstring GetOriginsPath(const std::wstring& spaceFolder) const;

    std::wstring m_appRoot;
    std::wstring m_spacesRoot;
    std::wstring m_metadataPath;
};
