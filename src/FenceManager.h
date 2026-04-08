#pragma once
#include <functional>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include "Models.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginContracts.h"

class FenceWindow;
class FenceStorage;
class Persistence;
class SpaceExtensionRegistry;
class ThemePlatform;

class FenceManager
{
public:
    FenceManager(std::unique_ptr<FenceStorage> storage, std::unique_ptr<Persistence> persistence);
    ~FenceManager();

    bool LoadAll();
    bool SaveAll(const std::wstring& correlationId = L"", const std::wstring& reason = L"");

    std::wstring CreateFenceAt(int x, int y, const std::wstring& title = L"New Fence");
    std::wstring CreateFenceAt(int x, int y, const FenceCreateRequest& request);
    void DeleteFence(const std::wstring& fenceId, const std::wstring& correlationId = L"");
    void RenameFence(const std::wstring& fenceId, const std::wstring& newTitle);

    void RefreshFence(const std::wstring& fenceId);
    void RefreshAll();

    bool HandleDrop(const std::wstring& fenceId, const std::vector<std::wstring>& paths, const std::wstring& correlationId = L"");
    bool DeleteItem(const std::wstring& fenceId, const FenceItem& item);
    void SetFenceTextOnlyMode(const std::wstring& fenceId, bool enabled);
    void SetFenceThemePolicyInheritance(const std::wstring& fenceId, bool enabled);
    void SetFenceRollupWhenNotHovered(const std::wstring& fenceId, bool enabled, const std::wstring& correlationId = L"");
    void SetFenceTransparentWhenNotHovered(const std::wstring& fenceId, bool enabled, const std::wstring& correlationId = L"");
    void SetFenceLabelsOnHover(const std::wstring& fenceId, bool enabled);
    void SetFenceIconSpacingPreset(const std::wstring& fenceId, const std::wstring& preset);
    void ApplyFenceSettingsToAll(const std::wstring& sourceFenceId);
    void UpdateFenceGeometry(const std::wstring& fenceId,
                            int x,
                            int y,
                            int width,
                            int height,
                            const std::wstring& correlationId = L"");
    void SetAllFencesHidden(bool hidden);
    void ToggleAllFencesHidden();
    bool AreAllFencesHidden() const;
    void Shutdown();
    FenceModel* FindFence(const std::wstring& fenceId);
    const FenceModel* FindFence(const std::wstring& fenceId) const;
    FenceWindow* FindFenceWindow(const std::wstring& fenceId);
    const FenceWindow* FindFenceWindow(const std::wstring& fenceId) const;
    const FenceModel* FindFenceByWindow(HWND hwnd) const;
    std::vector<std::wstring> GetAllFenceIds() const;
    void SetSpaceExtensionRegistry(const SpaceExtensionRegistry* registry);
    void SetMenuContributionRegistry(const MenuContributionRegistry* registry);
    void SetCommandExecutor(std::function<bool(const std::wstring&, const CommandContext&)> executor);
    void SetThemePlatform(const ThemePlatform* themePlatform);
    bool SetFenceContentSource(const std::wstring& fenceId, const std::wstring& contentSource);
    void SetFenceContentState(const std::wstring& fenceId,
                              const std::wstring& state,
                              const std::wstring& detail);
    void ApplyFencePresentation(const std::wstring& fenceId, const FencePresentationSettings& settings);
    std::vector<MenuContribution> GetMenuContributions(MenuSurface surface) const;
    bool ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const;

private:
    std::wstring GenerateFenceId() const;
    bool NormalizeFenceContentProvider(FenceModel& fence) const;
    FenceMetadata BuildFenceMetadata(const FenceModel& fence) const;
    bool PersistWithTrace(const std::wstring& reason, const std::wstring& correlationId);

    std::unique_ptr<FenceStorage> m_storage;
    std::unique_ptr<Persistence> m_persistence;
    std::vector<FenceModel> m_fences;
    std::unordered_map<std::wstring, std::unique_ptr<FenceWindow>> m_windows;
    const SpaceExtensionRegistry* m_spaceExtensionRegistry = nullptr;
    const MenuContributionRegistry* m_menuRegistry = nullptr;
    const ThemePlatform* m_themePlatform = nullptr;
    std::function<bool(const std::wstring&, const CommandContext&)> m_commandExecutor;
    bool m_allFencesHidden = false;
};
