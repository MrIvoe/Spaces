#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include "Models.h"

class FenceStorage
{
public:
    FenceStorage();
    ~FenceStorage() = default;

    std::wstring GetAppRoot() const;
    std::wstring GetFencesRoot() const;
    std::wstring GetMetadataPath() const;

    std::wstring EnsureFenceFolder(const std::wstring& fenceId);
    std::vector<FenceItem> ScanFenceItems(const std::wstring& folder) const;
    FileMoveResult MovePathsIntoFence(const std::vector<std::wstring>& sourcePaths, const std::wstring& fenceFolder);
    bool RestoreItemToOrigin(const std::wstring& fenceFolder,
                             const FenceItem& item,
                             std::wstring* failureReason = nullptr,
                             std::filesystem::path* restoredDestination = nullptr);
    RestoreResult RestoreAllItems(const std::wstring& fenceFolder);
    bool DeleteItem(const std::wstring& fenceFolder, const FenceItem& item);
    bool DeleteFenceFolderIfEmpty(const std::wstring& fenceFolder);
    bool MarkFenceDeleted(const std::wstring& fenceFolder);
    bool IsFenceDeletedMarked(const std::wstring& fenceFolder) const;
    bool ClearFenceDeletedMarker(const std::wstring& fenceFolder);

    // Icon utilities
    static int GetFileIconIndex(const std::wstring& filePath);

private:
    std::filesystem::path GenerateNonConflictingPath(const std::filesystem::path& target) const;
    void SaveItemOrigins(const std::wstring& fenceFolder, const std::map<std::wstring, std::wstring>& origins);
    std::map<std::wstring, std::wstring> LoadItemOrigins(const std::wstring& fenceFolder) const;
    std::wstring GetOriginsPath(const std::wstring& fenceFolder) const;

    std::wstring m_appRoot;
    std::wstring m_fencesRoot;
    std::wstring m_metadataPath;
};
