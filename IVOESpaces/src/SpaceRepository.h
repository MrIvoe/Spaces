#pragma once

#include <filesystem>
#include <vector>

#include "SpaceModels.h"

struct SpaceRepositoryState {
    std::vector<SpaceData> spaces;
    DropPolicy dropPolicy{DropPolicy::Move};
    bool showInfoNotifications{true};
    std::wstring monitorSignature;
};

class SpaceRepository {
public:
    SpaceRepository();
    explicit SpaceRepository(std::filesystem::path basePath);

    bool Load(SpaceRepositoryState& state) const;
    bool Save(const SpaceRepositoryState& state) const;

    std::filesystem::path GetBasePath() const { return m_basePath; }
    std::filesystem::path GetConfigPath() const;
    std::filesystem::path GetSpaceDataRoot() const;
    std::filesystem::path GetLegacyIniPath() const;

    static std::wstring BuildMonitorSignature();

private:
    bool LoadFromJson(SpaceRepositoryState& state) const;
    bool LoadFromLegacyIni(SpaceRepositoryState& state) const;
    bool MigrateLegacyFolderBackedSpaces(SpaceRepositoryState& state) const;
    static std::vector<DesktopItemRef> ScanFolderMembers(const std::wstring& folderPath);
    static DropPolicy ParseDropPolicy(int value);

    std::filesystem::path m_basePath;
};