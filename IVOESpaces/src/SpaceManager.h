#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "SpaceModels.h"

class SpaceWindow;
class SpaceRepository;

class SpaceManager {
public:
    using StatusCallback = std::function<void(const std::wstring&, const std::wstring&, bool)>;

    SpaceManager(HINSTANCE instance, HWND desktopHost);
    ~SpaceManager();

    bool Initialize();
    void RestoreOrCreateDefaultSpace();
    void CreateSpaceAt(POINT screenPt, bool notifyUser = true);
    void CreatePortalSpaceAt(POINT screenPt, const std::wstring& folderPath);
    void ExitApplication();
    void SaveAll();
    void OnShellChanged();
    void MaintainDesktopPlacement();
    void RefreshAllSpaceWindows();
    void HandleDroppedFiles(int spaceId, const std::vector<std::wstring>& paths);
    bool OpenFirstSpaceBackingFolder();
    bool OpenSpaceBackingFolder(int spaceId);
    bool DeleteSpace(int spaceId);
    bool CanDeleteSpace(int spaceId) const;
    DropPolicy GetDropPolicy() const { return m_dropPolicy; }
    void SetDropPolicy(DropPolicy policy);
    bool GetShowInfoNotifications() const { return m_showInfoNotifications; }
    void SetShowInfoNotifications(bool enabled);
    void SetStatusCallback(StatusCallback callback);

private:
    void CreateSpaceFromData(const SpaceData& data);
    SpaceData* FindSpaceRecord(int spaceId);
    const SpaceData* FindSpaceRecord(int spaceId) const;
    std::wstring EnsureSpaceBackingFolder(SpaceWindow& space);
    void RefreshSpaceItems(SpaceWindow& space, SpaceData* record);
    void SaveRepositoryState();
    void NotifyStatus(const std::wstring& title, const std::wstring& message, bool error) const;

    HINSTANCE m_instance{};
    HWND m_desktopHost{};
    int m_nextId{1};
    DropPolicy m_dropPolicy{DropPolicy::Move};
    bool m_showInfoNotifications{true};
    StatusCallback m_statusCallback;
    std::unique_ptr<SpaceRepository> m_repository;
    std::vector<SpaceData> m_savedSpaces;
    std::unordered_map<int, SpaceData> m_spaceRecords;
    std::vector<std::unique_ptr<SpaceWindow>> m_spaces;
};
