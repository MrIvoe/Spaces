#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Models.h"

class FenceWindow;
class FenceStorage;
class Persistence;
class FenceExtensionRegistry;

class FenceManager
{
public:
    FenceManager(std::unique_ptr<FenceStorage> storage, std::unique_ptr<Persistence> persistence);
    ~FenceManager();

    bool LoadAll();
    bool SaveAll();

    std::wstring CreateFenceAt(int x, int y, const std::wstring& title = L"New Fence");
    void DeleteFence(const std::wstring& fenceId);
    void RenameFence(const std::wstring& fenceId, const std::wstring& newTitle);

    void RefreshFence(const std::wstring& fenceId);
    void RefreshAll();

    bool HandleDrop(const std::wstring& fenceId, const std::vector<std::wstring>& paths);
    bool DeleteItem(const std::wstring& fenceId, const FenceItem& item);
    void UpdateFenceGeometry(const std::wstring& fenceId, int x, int y, int width, int height);
    void Shutdown();
    FenceModel* FindFence(const std::wstring& fenceId);
    FenceWindow* FindFenceWindow(const std::wstring& fenceId);
    void SetFenceExtensionRegistry(const FenceExtensionRegistry* registry);

private:
    std::wstring GenerateFenceId() const;
    bool NormalizeFenceContentProvider(FenceModel& fence) const;

private:
    std::unique_ptr<FenceStorage> m_storage;
    std::unique_ptr<Persistence> m_persistence;
    std::vector<FenceModel> m_fences;
    std::unordered_map<std::wstring, std::unique_ptr<FenceWindow>> m_windows;
    const FenceExtensionRegistry* m_fenceExtensionRegistry = nullptr;
};
