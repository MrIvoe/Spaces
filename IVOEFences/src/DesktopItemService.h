#pragma once

#include <filesystem>
#include <vector>

#include "FenceModels.h"

class DesktopItemService {
public:
    bool Initialize();
    std::filesystem::path GetDesktopPath() const { return m_desktopPath; }

    std::vector<DesktopItemRef> EnumerateDesktopItems() const;
    std::vector<DesktopItemRef> EnumerateFolderItems(const std::wstring& folderPath, DesktopItemSource source) const;
    std::vector<DesktopItemRef> BuildLegacyFolderMembership(const std::wstring& folderPath) const;

private:
    std::filesystem::path m_desktopPath;
};