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

class SpaceWindow;
class SpaceStorage;
class Persistence;
class SpaceExtensionRegistry;
class ThemePlatform;

class SpaceManager
{
public:
    SpaceManager(std::unique_ptr<SpaceStorage> storage, std::unique_ptr<Persistence> persistence);
    ~SpaceManager();

    bool LoadAll();
    bool SaveAll(const std::wstring& correlationId = L"", const std::wstring& reason = L"");

    std::wstring CreateSpaceAt(int x, int y, const std::wstring& title = L"New Space");
    std::wstring CreateSpaceAt(int x, int y, const SpaceCreateRequest& request);
    void DeleteSpace(const std::wstring& spaceId, const std::wstring& correlationId = L"");
    void RenameSpace(const std::wstring& spaceId, const std::wstring& newTitle);

    void RefreshSpace(const std::wstring& spaceId);
    void RefreshAll();

    bool HandleDrop(const std::wstring& spaceId, const std::vector<std::wstring>& paths, const std::wstring& correlationId = L"");
    bool DeleteItem(const std::wstring& spaceId, const SpaceItem& item);
    void SetSpaceTextOnlyMode(const std::wstring& spaceId, bool enabled);
    void SetSpaceThemePolicyInheritance(const std::wstring& spaceId, bool enabled);
    void SetSpaceRollupWhenNotHovered(const std::wstring& spaceId, bool enabled, const std::wstring& correlationId = L"");
    void SetSpaceTransparentWhenNotHovered(const std::wstring& spaceId, bool enabled, const std::wstring& correlationId = L"");
    void SetSpaceLabelsOnHover(const std::wstring& spaceId, bool enabled);
    void SetSpaceIconSpacingPreset(const std::wstring& spaceId, const std::wstring& preset);
    void ApplySpaceSettingsToAll(const std::wstring& sourceSpaceId);
    void UpdateSpaceGeometry(const std::wstring& spaceId,
                            int x,
                            int y,
                            int width,
                            int height,
                            const std::wstring& correlationId = L"");
    void SetAllSpacesHidden(bool hidden);
    void ToggleAllSpacesHidden();
    bool AreAllSpacesHidden() const;
    void Shutdown();
    SpaceModel* FindSpace(const std::wstring& spaceId);
    const SpaceModel* FindSpace(const std::wstring& spaceId) const;
    SpaceWindow* FindSpaceWindow(const std::wstring& spaceId);
    const SpaceWindow* FindSpaceWindow(const std::wstring& spaceId) const;
    const SpaceModel* FindSpaceByWindow(HWND hwnd) const;
    std::vector<std::wstring> GetAllSpaceIds() const;
    void SetSpaceExtensionRegistry(const SpaceExtensionRegistry* registry);
    void SetMenuContributionRegistry(const MenuContributionRegistry* registry);
    void SetCommandExecutor(std::function<bool(const std::wstring&, const CommandContext&)> executor);
    void SetThemePlatform(const ThemePlatform* themePlatform);
    void SetSettingReader(std::function<std::wstring(const std::wstring&, const std::wstring&)> reader);
    bool SetSpaceContentSource(const std::wstring& spaceId, const std::wstring& contentSource);
    void SetSpaceContentState(const std::wstring& spaceId,
                              const std::wstring& state,
                              const std::wstring& detail);
    void ApplySpacePresentation(const std::wstring& spaceId, const SpacePresentationSettings& settings);
    std::vector<MenuContribution> GetMenuContributions(MenuSurface surface) const;
    bool ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const;

private:
    std::wstring GenerateSpaceId() const;
    bool NormalizeSpaceContentProvider(SpaceModel& space) const;
    SpaceMetadata BuildSpaceMetadata(const SpaceModel& space) const;
    bool PersistWithTrace(const std::wstring& reason, const std::wstring& correlationId);

    std::unique_ptr<SpaceStorage> m_storage;
    std::unique_ptr<Persistence> m_persistence;
    std::vector<SpaceModel> m_spaces;
    std::unordered_map<std::wstring, std::unique_ptr<SpaceWindow>> m_windows;
    const SpaceExtensionRegistry* m_spaceExtensionRegistry = nullptr;
    const MenuContributionRegistry* m_menuRegistry = nullptr;
    const ThemePlatform* m_themePlatform = nullptr;
    std::function<bool(const std::wstring&, const CommandContext&)> m_commandExecutor;
    std::function<std::wstring(const std::wstring&, const std::wstring&)> m_settingReader;
    bool m_allSpacesHidden = false;
};
