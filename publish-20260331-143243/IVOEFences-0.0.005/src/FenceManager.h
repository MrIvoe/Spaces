#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "FenceModels.h"

class FenceWindow;
class FenceRepository;

class FenceManager {
public:
    using StatusCallback = std::function<void(const std::wstring&, const std::wstring&, bool)>;

    FenceManager(HINSTANCE instance, HWND desktopHost);
    ~FenceManager();

    bool Initialize();
    void RestoreOrCreateDefaultFence();
    void CreateFenceAt(POINT screenPt, bool notifyUser = true);
    void ExitApplication();
    void SaveAll();
    void OnShellChanged();
    void MaintainDesktopPlacement();
    void HandleDroppedFiles(int fenceId, const std::vector<std::wstring>& paths);
    bool OpenFirstFenceBackingFolder();
    bool OpenFenceBackingFolder(int fenceId);
    bool DeleteFence(int fenceId);
    bool CanDeleteFence(int fenceId) const;
    DropPolicy GetDropPolicy() const { return m_dropPolicy; }
    void SetDropPolicy(DropPolicy policy);
    bool GetShowInfoNotifications() const { return m_showInfoNotifications; }
    void SetShowInfoNotifications(bool enabled);
    void SetStatusCallback(StatusCallback callback);

private:
    void CreateFenceFromData(const FenceData& data);
    FenceData* FindFenceRecord(int fenceId);
    const FenceData* FindFenceRecord(int fenceId) const;
    std::wstring EnsureFenceBackingFolder(FenceWindow& fence);
    void RefreshFenceItems(FenceWindow& fence, FenceData* record);
    void SaveRepositoryState();
    void NotifyStatus(const std::wstring& title, const std::wstring& message, bool error) const;

    HINSTANCE m_instance{};
    HWND m_desktopHost{};
    int m_nextId{1};
    DropPolicy m_dropPolicy{DropPolicy::Move};
    bool m_showInfoNotifications{true};
    StatusCallback m_statusCallback;
    std::unique_ptr<FenceRepository> m_repository;
    std::vector<FenceData> m_savedFences;
    std::unordered_map<int, FenceData> m_fenceRecords;
    std::vector<std::unique_ptr<FenceWindow>> m_fences;
};
