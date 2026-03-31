#pragma once

#include <filesystem>
#include <vector>

#include "FenceModels.h"

struct FenceRepositoryState {
    std::vector<FenceData> fences;
    DropPolicy dropPolicy{DropPolicy::Move};
    bool showInfoNotifications{true};
    std::wstring monitorSignature;
};

class FenceRepository {
public:
    FenceRepository();
    explicit FenceRepository(std::filesystem::path basePath);

    bool Load(FenceRepositoryState& state) const;
    bool Save(const FenceRepositoryState& state) const;

    std::filesystem::path GetBasePath() const { return m_basePath; }
    std::filesystem::path GetConfigPath() const;
    std::filesystem::path GetFenceDataRoot() const;
    std::filesystem::path GetLegacyIniPath() const;

    static std::wstring BuildMonitorSignature();

private:
    bool LoadFromJson(FenceRepositoryState& state) const;
    bool LoadFromLegacyIni(FenceRepositoryState& state) const;
    bool MigrateLegacyFolderBackedFences(FenceRepositoryState& state) const;
    static std::vector<DesktopItemRef> ScanFolderMembers(const std::wstring& folderPath);
    static DropPolicy ParseDropPolicy(int value);

    std::filesystem::path m_basePath;
};