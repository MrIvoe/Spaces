#include "SpaceManager.h"
#include "SpaceWindow.h"
#include "SpaceStorage.h"
#include "Persistence.h"
#include "Win32Helpers.h"
#include "core/ThemePlatform.h"
#include "extensions/SpaceExtensionRegistry.h"
#include <algorithm>
#include <cwctype>
#include <unordered_set>
#include <objbase.h>
#include <ole2.h>
#include <sstream>

namespace
{
    constexpr int kMinSpaceWidth = 120;
    constexpr int kMinSpaceHeight = 60;

    bool ClampSpaceGeometry(SpaceModel& space)
    {
        bool changed = false;

        if (space.width < kMinSpaceWidth)
        {
            space.width = kMinSpaceWidth;
            changed = true;
        }

        if (space.height < kMinSpaceHeight)
        {
            space.height = kMinSpaceHeight;
            changed = true;
        }

        return changed;
    }

    std::wstring CorrelationPrefix(const std::wstring& correlationId)
    {
        return correlationId.empty() ? L"" : (L"[cid=" + correlationId + L"] ");
    }

    std::wstring ToLower(const std::wstring& value)
    {
        std::wstring lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
        return lowered;
    }

    bool IsSameOrChildPath(const std::filesystem::path& root, const std::filesystem::path& candidate)
    {
        std::error_code ec;
        const auto canonicalRoot = std::filesystem::weakly_canonical(root, ec);
        if (ec)
        {
            return false;
        }

        ec.clear();
        const auto canonicalCandidate = std::filesystem::weakly_canonical(candidate, ec);
        if (ec)
        {
            return false;
        }

        const auto rootText = ToLower(canonicalRoot.wstring());
        const auto candidateText = ToLower(canonicalCandidate.wstring());
        if (candidateText == rootText)
        {
            return true;
        }

        return candidateText.size() > rootText.size() &&
               candidateText.compare(0, rootText.size(), rootText) == 0 &&
               (candidateText[rootText.size()] == L'\\' || candidateText[rootText.size()] == L'/');
    }
}

SpaceManager::SpaceManager(std::unique_ptr<SpaceStorage> storage, std::unique_ptr<Persistence> persistence)
    : m_storage(std::move(storage)), m_persistence(std::move(persistence))
{
}

SpaceManager::~SpaceManager() = default;

bool SpaceManager::LoadAll()
{
    if (!m_persistence->LoadSpaces(m_spaces))
        return false;

    std::vector<SpaceModel> activeSpaces;
    activeSpaces.reserve(m_spaces.size());
    for (const auto& loadedSpace : m_spaces)
    {
        if (!loadedSpace.backingFolder.empty() && m_storage->IsSpaceDeletedMarked(loadedSpace.backingFolder))
        {
            Win32Helpers::LogInfo(L"[SpaceManager] Skipping tombstoned space during load: spaceId='" + loadedSpace.id + L"'");
            continue;
        }

        activeSpaces.push_back(loadedSpace);
    }

    if (activeSpaces.size() != m_spaces.size())
    {
        m_spaces = std::move(activeSpaces);
        PersistWithTrace(L"drop_tombstoned_spaces_on_load", L"");
    }

    static constexpr size_t kMaxLoadedSpacesForSession = 16;
    if (m_spaces.size() > kMaxLoadedSpacesForSession)
    {
        const size_t trimCount = m_spaces.size() - kMaxLoadedSpacesForSession;
        m_spaces.erase(m_spaces.begin(), m_spaces.begin() + static_cast<std::ptrdiff_t>(trimCount));
        Win32Helpers::LogInfo(L"Space load trimmed to last " + std::to_wstring(kMaxLoadedSpacesForSession) +
                              L" entries for test stability.");
        PersistWithTrace(L"load_trim", L"");
    }

    bool normalizedAny = false;

    // Create windows for each loaded space
    for (auto& space : m_spaces)
    {
        if (ClampSpaceGeometry(space))
        {
            normalizedAny = true;
            Win32Helpers::LogInfo(L"[SpaceManager] Clamped loaded geometry spaceId='" + space.id +
                                  L"' width=" + std::to_wstring(space.width) +
                                  L" height=" + std::to_wstring(space.height));
        }

        normalizedAny = NormalizeSpaceContentProvider(space) || normalizedAny;

        // Ensure backing folder exists
        if (space.backingFolder.empty())
        {
            space.backingFolder = m_storage->EnsureSpaceFolder(space.id);
        }
        else
        {
            m_storage->EnsureSpaceFolder(space.id);
        }

        auto window = std::make_unique<SpaceWindow>(this, space, m_themePlatform);
        if (window->Create())
        {
            window->Show();
            m_windows[space.id] = std::move(window);

            // Keep file-collection spaces eager, but avoid blocking startup on
            // portal/provider enumeration. Portal content is loaded by the
            // background watcher on its first pass.
            if (space.contentType != L"folder_portal")
            {
                RefreshSpace(space.id);
            }
        }
    }

    if (normalizedAny)
    {
        Win32Helpers::LogInfo(L"Persisting normalized space metadata after load.");
        PersistWithTrace(L"load_normalized_metadata", L"");
    }

    return true;
}

bool SpaceManager::SaveAll(const std::wstring& correlationId, const std::wstring& reason)
{
    return PersistWithTrace(reason.empty() ? L"save_all" : reason, correlationId);
}

std::wstring SpaceManager::GenerateSpaceId() const
{
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t buffer[64]{};
    StringFromGUID2(guid, buffer, 64);
    return buffer;
}

std::wstring SpaceManager::CreateSpaceAt(int x, int y, const std::wstring& title)
{
    SpaceCreateRequest request;
    request.title = title;
    return CreateSpaceAt(x, y, request);
}

std::wstring SpaceManager::CreateSpaceAt(int x, int y, const SpaceCreateRequest& request)
{
    SpaceModel space;
    space.id = GenerateSpaceId();
    space.title = request.title.empty() ? L"New Space" : request.title;
    space.x = x;
    space.y = y;
    space.width = request.width;
    space.height = request.height;
    ClampSpaceGeometry(space);
    space.backingFolder = m_storage->EnsureSpaceFolder(space.id);
    m_storage->ClearSpaceDeletedMarker(space.backingFolder);
    space.contentType = request.contentType.empty() ? L"file_collection" : request.contentType;
    space.contentPluginId = request.contentPluginId.empty() ? L"core.file_collection" : request.contentPluginId;
    space.contentSource = request.contentSource;
    space.contentState = request.contentSource.empty() ? L"ready" : L"connecting";

    NormalizeSpaceContentProvider(space);

    m_spaces.push_back(space);

    auto window = std::make_unique<SpaceWindow>(this, space, m_themePlatform);
    if (!window->Create())
        return L"";

    window->Show();
    m_windows[space.id] = std::move(window);

    if (space.contentType != L"folder_portal")
    {
        // Populate normal space content immediately for consistency with startup load.
        RefreshSpace(space.id);
    }

    if (m_allSpacesHidden)
    {
        ShowWindow(m_windows[space.id]->GetHwnd(), SW_HIDE);
    }

    // Persist immediately
    PersistWithTrace(L"create_space", L"");

    return space.id;
}

void SpaceManager::DeleteSpace(const std::wstring& spaceId, const std::wstring& correlationId)
{
    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] DeleteSpace requested spaceId='" + spaceId + L"'");

    // Find and restore items to original locations
    auto space = FindSpace(spaceId);
    if (space)
    {
        if (!space->backingFolder.empty())
        {
            m_storage->MarkSpaceDeleted(space->backingFolder);
        }

        const RestoreResult restore = m_storage->RestoreAllItems(space->backingFolder);
        if (!restore.AllSucceeded())
        {
            Win32Helpers::LogInfo(
                CorrelationPrefix(correlationId) +
                L"Delete space continuing despite partial restore failures: spaceId='" + spaceId +
                L"' restored=" + std::to_wstring(restore.restoredCount) +
                L" failed=" + std::to_wstring(restore.failedCount));

            for (const auto& failed : restore.failedItems)
            {
                Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                                      L"Restore failure detail: path='" + failed.first.wstring() +
                                      L"' reason='" + failed.second + L"'");
            }

            // Keep the backing folder when restore is partial, but still honor user delete.
            // This prevents "deleted" spaces from reappearing on next launch.
        }

        if (restore.AllSucceeded())
        {
            m_storage->DeleteSpaceFolderIfEmpty(space->backingFolder);
        }
    }

    // Remove window
    auto it = m_windows.find(spaceId);
    if (it != m_windows.end())
    {
        it->second->Destroy();
        m_windows.erase(it);
    }

    // Remove from model list
    m_spaces.erase(
        std::remove_if(m_spaces.begin(), m_spaces.end(),
            [&](const SpaceModel& f) { return f.id == spaceId; }),
        m_spaces.end());

    // Persist
    PersistWithTrace(L"delete_space", correlationId);
}

void SpaceManager::SetAllSpacesHidden(bool hidden)
{
    if (m_allSpacesHidden == hidden)
    {
        return;
    }

    m_allSpacesHidden = hidden;
    for (auto& entry : m_windows)
    {
        if (!entry.second)
        {
            continue;
        }

        ShowWindow(entry.second->GetHwnd(), hidden ? SW_HIDE : SW_SHOWNOACTIVATE);
    }

    Win32Helpers::LogInfo(L"[SpaceManager] All space visibility changed: hidden=" + std::wstring(hidden ? L"true" : L"false"));
}

void SpaceManager::ToggleAllSpacesHidden()
{
    SetAllSpacesHidden(!m_allSpacesHidden);
}

bool SpaceManager::AreAllSpacesHidden() const
{
    return m_allSpacesHidden;
}

void SpaceManager::RenameSpace(const std::wstring& spaceId, const std::wstring& newTitle)
{
    auto space = FindSpace(spaceId);
    if (!space)
        return;

    space->title = newTitle;

    auto window = FindSpaceWindow(spaceId);
    if (window)
        window->UpdateModel(*space);

    // Persist
    PersistWithTrace(L"rename_space", L"");
}

void SpaceManager::RefreshSpace(const std::wstring& spaceId)
{
    auto space = FindSpace(spaceId);
    if (!space)
        return;

    std::vector<SpaceItem> items;
    const SpaceContentProviderCallbacks* callbacks = m_spaceExtensionRegistry
        ? m_spaceExtensionRegistry->ResolveCallbacks(space->contentType, space->contentPluginId)
        : nullptr;
    if (callbacks && callbacks->enumerateItems)
    {
        items = callbacks->enumerateItems(BuildSpaceMetadata(*space));
    }
    else
    {
        items = m_storage->ScanSpaceItems(space->backingFolder);
    }

    auto window = FindSpaceWindow(spaceId);
    if (window)
        window->SetItems(items);
}

void SpaceManager::RefreshAll()
{
    for (auto& space : m_spaces)
    {
        std::vector<SpaceItem> items;
        const SpaceContentProviderCallbacks* callbacks = m_spaceExtensionRegistry
            ? m_spaceExtensionRegistry->ResolveCallbacks(space.contentType, space.contentPluginId)
            : nullptr;
        if (callbacks && callbacks->enumerateItems)
        {
            items = callbacks->enumerateItems(BuildSpaceMetadata(space));
        }
        else
        {
            items = m_storage->ScanSpaceItems(space.backingFolder);
        }

        auto window = FindSpaceWindow(space.id);
        if (window)
            window->SetItems(items);
    }
}

bool SpaceManager::HandleDrop(const std::wstring& spaceId,
                              const std::vector<std::wstring>& paths,
                              const std::wstring& correlationId)
{
    auto space = FindSpace(spaceId);
    if (!space)
        return false;

    const SpaceContentProviderCallbacks* callbacks = m_spaceExtensionRegistry
        ? m_spaceExtensionRegistry->ResolveCallbacks(space->contentType, space->contentPluginId)
        : nullptr;
    if (callbacks && callbacks->handleDrop)
    {
        const bool handled = callbacks->handleDrop(BuildSpaceMetadata(*space), paths);
        if (handled)
        {
            RefreshSpace(spaceId);
        }
        return handled;
    }

    std::vector<std::wstring> filtered;
    filtered.reserve(paths.size());

    std::unordered_set<std::wstring> seen;
    const std::filesystem::path backingFolderPath(space->backingFolder);

    for (const auto& rawPath : paths)
    {
        if (rawPath.empty())
        {
            continue;
        }

        std::filesystem::path sourcePath(rawPath);
        if (IsSameOrChildPath(backingFolderPath, sourcePath))
        {
            Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                                  L"Drop skipped because path is already inside space folder: " + rawPath);
            continue;
        }

        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(sourcePath, ec);
        const std::wstring canonicalKey = ToLower((ec ? sourcePath.wstring() : canonical.wstring()));
        if (seen.find(canonicalKey) != seen.end())
        {
            continue;
        }

        seen.insert(canonicalKey);
        filtered.push_back(rawPath);
    }

    if (filtered.empty())
    {
        return false;
    }

    FileMoveResult result = m_storage->MovePathsIntoSpace(filtered, space->backingFolder);
    for (const auto& failure : result.failed)
    {
        Win32Helpers::LogError(CorrelationPrefix(correlationId) +
                               L"Drop failed for path: " + failure.first.wstring() +
                               L" reason: " + failure.second);
    }

    if (result.moved.empty())
    {
        return false;
    }

    RefreshSpace(spaceId);
    return !result.HasFailures();
}

bool SpaceManager::DeleteItem(const std::wstring& spaceId, const SpaceItem& item)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return false;
    }

    const SpaceContentProviderCallbacks* callbacks = m_spaceExtensionRegistry
        ? m_spaceExtensionRegistry->ResolveCallbacks(space->contentType, space->contentPluginId)
        : nullptr;
    const bool ok = (callbacks && callbacks->deleteItem)
        ? callbacks->deleteItem(BuildSpaceMetadata(*space), item)
        : m_storage->DeleteItem(space->backingFolder, item);
    if (ok)
    {
        RefreshSpace(spaceId);
    }
    return ok;
}

void SpaceManager::SetSpaceTextOnlyMode(const std::wstring& spaceId, bool enabled)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    space->textOnlyMode = enabled;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::SetSpaceThemePolicyInheritance(const std::wstring& spaceId, bool enabled)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    space->inheritThemePolicy = enabled;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::SetSpaceRollupWhenNotHovered(const std::wstring& spaceId,
                                                bool enabled,
                                                const std::wstring& correlationId)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] SetSpaceRollupWhenNotHovered spaceId='" + spaceId +
                          L"' enabled=" + (enabled ? L"true" : L"false"));

    space->rollupWhenNotHovered = enabled;
    space->inheritThemePolicy = false;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll(correlationId, L"set_rollup_policy");
}

void SpaceManager::SetSpaceTransparentWhenNotHovered(const std::wstring& spaceId,
                                                     bool enabled,
                                                     const std::wstring& correlationId)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] SetSpaceTransparentWhenNotHovered spaceId='" + spaceId +
                          L"' enabled=" + (enabled ? L"true" : L"false"));

    space->transparentWhenNotHovered = enabled;
    space->inheritThemePolicy = false;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll(correlationId, L"set_transparency_policy");
}

void SpaceManager::SetSpaceLabelsOnHover(const std::wstring& spaceId, bool enabled)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    space->labelsOnHover = enabled;
    space->inheritThemePolicy = false;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::SetSpaceIconSpacingPreset(const std::wstring& spaceId, const std::wstring& preset)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    if (preset != L"compact" && preset != L"comfortable" && preset != L"spacious")
    {
        return;
    }

    space->iconSpacingPreset = preset;
    space->inheritThemePolicy = false;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::ApplySpaceSettingsToAll(const std::wstring& sourceSpaceId)
{
    SpaceModel* source = FindSpace(sourceSpaceId);
    if (!source)
    {
        return;
    }

    for (auto& space : m_spaces)
    {
        space.textOnlyMode = source->textOnlyMode;
        space.inheritThemePolicy = source->inheritThemePolicy;
        space.rollupWhenNotHovered = source->rollupWhenNotHovered;
        space.transparentWhenNotHovered = source->transparentWhenNotHovered;
        space.labelsOnHover = source->labelsOnHover;
        space.iconSpacingPreset = source->iconSpacingPreset;

        if (SpaceWindow* window = FindSpaceWindow(space.id))
        {
            window->UpdateModel(space);
        }
    }

    SaveAll();
}

void SpaceManager::UpdateSpaceGeometry(const std::wstring& spaceId,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       const std::wstring& correlationId)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] UpdateSpaceGeometry spaceId='" + spaceId +
                          L"' x=" + std::to_wstring(x) +
                          L" y=" + std::to_wstring(y) +
                          L" w=" + std::to_wstring(width) +
                          L" h=" + std::to_wstring(height));

    space->x = x;
    space->y = y;
    space->width = width;
    space->height = height;
    ClampSpaceGeometry(*space);
    SaveAll(correlationId, L"update_geometry");
}

void SpaceManager::Shutdown()
{
    SaveAll();

    for (auto& entry : m_windows)
    {
        if (entry.second)
        {
            entry.second->Destroy();
        }
    }

    m_windows.clear();
}

SpaceModel* SpaceManager::FindSpace(const std::wstring& spaceId)
{
    for (auto& space : m_spaces)
    {
        if (space.id == spaceId)
            return &space;
    }
    return nullptr;
}

const SpaceModel* SpaceManager::FindSpace(const std::wstring& spaceId) const
{
    for (const auto& space : m_spaces)
    {
        if (space.id == spaceId)
            return &space;
    }
    return nullptr;
}

SpaceWindow* SpaceManager::FindSpaceWindow(const std::wstring& spaceId)
{
    auto it = m_windows.find(spaceId);
    if (it != m_windows.end())
        return it->second.get();
    return nullptr;
}

const SpaceWindow* SpaceManager::FindSpaceWindow(const std::wstring& spaceId) const
{
    auto it = m_windows.find(spaceId);
    if (it != m_windows.end())
        return it->second.get();
    return nullptr;
}

const SpaceModel* SpaceManager::FindSpaceByWindow(HWND hwnd) const
{
    if (!hwnd)
    {
        return nullptr;
    }

    for (const auto& entry : m_windows)
    {
        if (entry.second && entry.second->GetHwnd() == hwnd)
        {
            return FindSpace(entry.first);
        }
    }

    return nullptr;
}

std::vector<std::wstring> SpaceManager::GetAllSpaceIds() const
{
    std::vector<std::wstring> ids;
    ids.reserve(m_spaces.size());
    for (const auto& space : m_spaces)
    {
        ids.push_back(space.id);
    }
    return ids;
}

void SpaceManager::SetSpaceExtensionRegistry(const SpaceExtensionRegistry* registry)
{
    m_spaceExtensionRegistry = registry;

    bool normalizedAny = false;
    for (auto& space : m_spaces)
    {
        normalizedAny = NormalizeSpaceContentProvider(space) || normalizedAny;
    }

    if (normalizedAny)
    {
        Win32Helpers::LogInfo(L"Persisting normalized space provider metadata after registry binding.");
        PersistWithTrace(L"normalize_provider_on_registry_bind", L"");
    }
}

void SpaceManager::SetMenuContributionRegistry(const MenuContributionRegistry* registry)
{
    m_menuRegistry = registry;
}

void SpaceManager::SetCommandExecutor(std::function<bool(const std::wstring&, const CommandContext&)> executor)
{
    m_commandExecutor = std::move(executor);
}

void SpaceManager::SetThemePlatform(const ThemePlatform* themePlatform)
{
    m_themePlatform = themePlatform;
}

bool SpaceManager::SetSpaceContentSource(const std::wstring& spaceId, const std::wstring& contentSource)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return false;
    }

    if (space->contentSource == contentSource)
    {
        return true;
    }

    space->contentSource = contentSource;
    space->contentState = contentSource.empty() ? L"needs_source" : L"connecting";
    space->contentStateDetail.clear();
    ClampSpaceGeometry(*space);
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();

    // Portal content is refreshed by the provider watcher thread; avoid a
    // synchronous enumerate on this path to keep UI interactions responsive.
    if (space->contentType != L"folder_portal")
    {
        RefreshSpace(spaceId);
    }
    return true;
}

void SpaceManager::SetSpaceContentState(const std::wstring& spaceId,
                                        const std::wstring& state,
                                        const std::wstring& detail)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    const std::wstring normalizedState = state.empty() ? L"ready" : state;
    if (space->contentState == normalizedState && space->contentStateDetail == detail)
    {
        return;
    }

    space->contentState = normalizedState;
    space->contentStateDetail = detail;
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::ApplySpacePresentation(const std::wstring& spaceId, const SpacePresentationSettings& settings)
{
    SpaceModel* sourceSpace = FindSpace(spaceId);
    if (!sourceSpace)
    {
        return;
    }

    auto applyToSpace = [&](SpaceModel& space) {
        if (settings.textOnlyMode.has_value())
        {
            space.textOnlyMode = *settings.textOnlyMode;
        }

        if (settings.rollupWhenNotHovered.has_value())
        {
            space.rollupWhenNotHovered = *settings.rollupWhenNotHovered;
            space.inheritThemePolicy = false;
        }
        if (settings.transparentWhenNotHovered.has_value())
        {
            space.transparentWhenNotHovered = *settings.transparentWhenNotHovered;
            space.inheritThemePolicy = false;
        }

        if (settings.labelsOnHover.has_value())
        {
            space.labelsOnHover = *settings.labelsOnHover;
            space.inheritThemePolicy = false;
        }
        if (settings.iconSpacingPreset.has_value())
        {
            const std::wstring& preset = *settings.iconSpacingPreset;
            if (preset == L"compact" || preset == L"comfortable" || preset == L"spacious")
            {
                space.iconSpacingPreset = preset;
                space.inheritThemePolicy = false;
            }
        }
    };

    if (settings.applyToAll)
    {
        for (auto& space : m_spaces)
        {
            applyToSpace(space);
            if (SpaceWindow* window = FindSpaceWindow(space.id))
            {
                window->UpdateModel(space);
            }
        }
    }
    else
    {
        applyToSpace(*sourceSpace);
        if (SpaceWindow* window = FindSpaceWindow(sourceSpace->id))
        {
            window->UpdateModel(*sourceSpace);
        }
    }

    SaveAll();
}

std::vector<MenuContribution> SpaceManager::GetMenuContributions(MenuSurface surface) const
{
    return m_menuRegistry ? m_menuRegistry->GetBySurface(surface) : std::vector<MenuContribution>{};
}

bool SpaceManager::ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const
{
    return m_commandExecutor ? m_commandExecutor(commandId, context) : false;
}

bool SpaceManager::PersistWithTrace(const std::wstring& reason, const std::wstring& correlationId)
{
    const bool saved = m_persistence->SaveSpaces(m_spaces);
    if (saved)
    {
        Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                              L"[SpaceManager] Persist success reason='" + reason +
                              L"' spaces=" + std::to_wstring(m_spaces.size()));
    }
    else
    {
        Win32Helpers::LogError(CorrelationPrefix(correlationId) +
                               L"[SpaceManager] Persist failed reason='" + reason + L"'");
    }

    return saved;
}

bool SpaceManager::NormalizeSpaceContentProvider(SpaceModel& space) const
{
    bool changed = false;

    if (!m_spaceExtensionRegistry)
    {
        if (space.contentType.empty())
        {
            space.contentType = L"file_collection";
            changed = true;
        }

        if (space.contentPluginId.empty())
        {
            space.contentPluginId = L"core.file_collection";
            changed = true;
        }
        return changed;
    }

    const bool supported = m_spaceExtensionRegistry->HasProvider(space.contentType, space.contentPluginId);
    if (supported)
    {
        return false;
    }

    const auto fallback = m_spaceExtensionRegistry->ResolveOrDefault(space.contentType, space.contentPluginId);
    Win32Helpers::LogError(
        L"Unsupported space provider detected. Falling back to core provider. spaceId='" + space.id +
        L"' contentType='" + space.contentType + L"' pluginId='" + space.contentPluginId + L"'");

    space.contentType = fallback.contentType;
    space.contentPluginId = fallback.providerId;
    return true;
}

SpaceMetadata SpaceManager::BuildSpaceMetadata(const SpaceModel& space) const
{
    SpaceMetadata meta;
    meta.id = space.id;
    meta.title = space.title;
    meta.backingFolderPath = space.backingFolder;
    meta.contentType = space.contentType;
    meta.contentPluginId = space.contentPluginId;
    meta.contentSource = space.contentSource;
    meta.contentState = space.contentState;
    meta.contentStateDetail = space.contentStateDetail;
    return meta;
}
