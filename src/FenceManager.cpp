#include "FenceManager.h"
#include "FenceWindow.h"
#include "FenceStorage.h"
#include "Persistence.h"
#include "Win32Helpers.h"
#include "core/ThemePlatform.h"
#include "extensions/FenceExtensionRegistry.h"
#include <algorithm>
#include <cwctype>
#include <unordered_set>
#include <objbase.h>
#include <ole2.h>
#include <sstream>

namespace
{
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

FenceManager::FenceManager(std::unique_ptr<FenceStorage> storage, std::unique_ptr<Persistence> persistence)
    : m_storage(std::move(storage)), m_persistence(std::move(persistence))
{
}

FenceManager::~FenceManager() = default;

bool FenceManager::LoadAll()
{
    if (!m_persistence->LoadFences(m_fences))
        return false;

    bool normalizedAny = false;

    // Create windows for each loaded fence
    for (auto& fence : m_fences)
    {
        normalizedAny = NormalizeFenceContentProvider(fence) || normalizedAny;

        // Ensure backing folder exists
        if (fence.backingFolder.empty())
        {
            fence.backingFolder = m_storage->EnsureFenceFolder(fence.id);
        }
        else
        {
            m_storage->EnsureFenceFolder(fence.id);
        }

        auto window = std::make_unique<FenceWindow>(this, fence, m_themePlatform);
        if (window->Create())
        {
            window->Show();
            m_windows[fence.id] = std::move(window);
        }
    }

    if (normalizedAny)
    {
        Win32Helpers::LogInfo(L"Persisting normalized fence provider metadata after load.");
        m_persistence->SaveFences(m_fences);
    }

    return true;
}

bool FenceManager::SaveAll()
{
    return m_persistence->SaveFences(m_fences);
}

std::wstring FenceManager::GenerateFenceId() const
{
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t buffer[64]{};
    StringFromGUID2(guid, buffer, 64);
    return buffer;
}

std::wstring FenceManager::CreateFenceAt(int x, int y, const std::wstring& title)
{
    FenceCreateRequest request;
    request.title = title;
    return CreateFenceAt(x, y, request);
}

std::wstring FenceManager::CreateFenceAt(int x, int y, const FenceCreateRequest& request)
{
    FenceModel fence;
    fence.id = GenerateFenceId();
    fence.title = request.title.empty() ? L"New Fence" : request.title;
    fence.x = x;
    fence.y = y;
    fence.width = 320;
    fence.height = 240;
    fence.backingFolder = m_storage->EnsureFenceFolder(fence.id);
    fence.contentType = request.contentType.empty() ? L"file_collection" : request.contentType;
    fence.contentPluginId = request.contentPluginId.empty() ? L"core.file_collection" : request.contentPluginId;
    fence.contentSource = request.contentSource;
    fence.contentState = request.contentSource.empty() ? L"ready" : L"connecting";

    NormalizeFenceContentProvider(fence);

    m_fences.push_back(fence);

    auto window = std::make_unique<FenceWindow>(this, fence, m_themePlatform);
    if (!window->Create())
        return L"";

    window->Show();
    m_windows[fence.id] = std::move(window);

    // Persist immediately
    m_persistence->SaveFences(m_fences);

    return fence.id;
}

void FenceManager::DeleteFence(const std::wstring& fenceId)
{
    // Find and restore items to original locations
    auto fence = FindFence(fenceId);
    if (fence)
    {
        const RestoreResult restore = m_storage->RestoreAllItems(fence->backingFolder);
        if (!restore.AllSucceeded())
        {
            Win32Helpers::LogError(
                L"Delete fence aborted due to partial restore failures: fenceId='" + fenceId +
                L"' restored=" + std::to_wstring(restore.restoredCount) +
                L" failed=" + std::to_wstring(restore.failedCount));

            for (const auto& failed : restore.failedItems)
            {
                Win32Helpers::LogError(L"Restore failure detail: path='" + failed.first.wstring() + L"' reason='" + failed.second + L"'");
            }

            RefreshFence(fenceId);
            return;
        }

        m_storage->DeleteFenceFolderIfEmpty(fence->backingFolder);
    }

    // Remove window
    auto it = m_windows.find(fenceId);
    if (it != m_windows.end())
    {
        it->second->Destroy();
        m_windows.erase(it);
    }

    // Remove from model list
    m_fences.erase(
        std::remove_if(m_fences.begin(), m_fences.end(),
            [&](const FenceModel& f) { return f.id == fenceId; }),
        m_fences.end());

    // Persist
    m_persistence->SaveFences(m_fences);
}

void FenceManager::RenameFence(const std::wstring& fenceId, const std::wstring& newTitle)
{
    auto fence = FindFence(fenceId);
    if (!fence)
        return;

    fence->title = newTitle;

    auto window = FindFenceWindow(fenceId);
    if (window)
        window->UpdateModel(*fence);

    // Persist
    m_persistence->SaveFences(m_fences);
}

void FenceManager::RefreshFence(const std::wstring& fenceId)
{
    auto fence = FindFence(fenceId);
    if (!fence)
        return;

    std::vector<FenceItem> items;
    const FenceContentProviderCallbacks* callbacks = m_fenceExtensionRegistry
        ? m_fenceExtensionRegistry->ResolveCallbacks(fence->contentType, fence->contentPluginId)
        : nullptr;
    if (callbacks && callbacks->enumerateItems)
    {
        items = callbacks->enumerateItems(BuildFenceMetadata(*fence));
    }
    else
    {
        items = m_storage->ScanFenceItems(fence->backingFolder);
    }

    auto window = FindFenceWindow(fenceId);
    if (window)
        window->SetItems(items);
}

void FenceManager::RefreshAll()
{
    for (auto& fence : m_fences)
    {
        std::vector<FenceItem> items;
        const FenceContentProviderCallbacks* callbacks = m_fenceExtensionRegistry
            ? m_fenceExtensionRegistry->ResolveCallbacks(fence.contentType, fence.contentPluginId)
            : nullptr;
        if (callbacks && callbacks->enumerateItems)
        {
            items = callbacks->enumerateItems(BuildFenceMetadata(fence));
        }
        else
        {
            items = m_storage->ScanFenceItems(fence.backingFolder);
        }

        auto window = FindFenceWindow(fence.id);
        if (window)
            window->SetItems(items);
    }
}

bool FenceManager::HandleDrop(const std::wstring& fenceId, const std::vector<std::wstring>& paths)
{
    auto fence = FindFence(fenceId);
    if (!fence)
        return false;

    const FenceContentProviderCallbacks* callbacks = m_fenceExtensionRegistry
        ? m_fenceExtensionRegistry->ResolveCallbacks(fence->contentType, fence->contentPluginId)
        : nullptr;
    if (callbacks && callbacks->handleDrop)
    {
        const bool handled = callbacks->handleDrop(BuildFenceMetadata(*fence), paths);
        if (handled)
        {
            RefreshFence(fenceId);
        }
        return handled;
    }

    std::vector<std::wstring> filtered;
    filtered.reserve(paths.size());

    std::unordered_set<std::wstring> seen;
    const std::filesystem::path backingFolderPath(fence->backingFolder);

    for (const auto& rawPath : paths)
    {
        if (rawPath.empty())
        {
            continue;
        }

        std::filesystem::path sourcePath(rawPath);
        if (IsSameOrChildPath(backingFolderPath, sourcePath))
        {
            Win32Helpers::LogInfo(L"Drop skipped because path is already inside fence folder: " + rawPath);
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

    FileMoveResult result = m_storage->MovePathsIntoFence(filtered, fence->backingFolder);
    for (const auto& failure : result.failed)
    {
        Win32Helpers::LogError(L"Drop failed for path: " + failure.first.wstring() + L" reason: " + failure.second);
    }

    if (result.moved.empty())
    {
        return false;
    }

    RefreshFence(fenceId);
    return !result.HasFailures();
}

bool FenceManager::DeleteItem(const std::wstring& fenceId, const FenceItem& item)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return false;
    }

    const FenceContentProviderCallbacks* callbacks = m_fenceExtensionRegistry
        ? m_fenceExtensionRegistry->ResolveCallbacks(fence->contentType, fence->contentPluginId)
        : nullptr;
    const bool ok = (callbacks && callbacks->deleteItem)
        ? callbacks->deleteItem(BuildFenceMetadata(*fence), item)
        : m_storage->DeleteItem(fence->backingFolder, item);
    if (ok)
    {
        RefreshFence(fenceId);
    }
    return ok;
}

void FenceManager::SetFenceTextOnlyMode(const std::wstring& fenceId, bool enabled)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->textOnlyMode = enabled;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::SetFenceThemePolicyInheritance(const std::wstring& fenceId, bool enabled)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->inheritThemePolicy = enabled;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::SetFenceRollupWhenNotHovered(const std::wstring& fenceId, bool enabled)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->rollupWhenNotHovered = enabled;
    fence->inheritThemePolicy = false;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::SetFenceTransparentWhenNotHovered(const std::wstring& fenceId, bool enabled)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->transparentWhenNotHovered = enabled;
    fence->inheritThemePolicy = false;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::SetFenceLabelsOnHover(const std::wstring& fenceId, bool enabled)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->labelsOnHover = enabled;
    fence->inheritThemePolicy = false;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::SetFenceIconSpacingPreset(const std::wstring& fenceId, const std::wstring& preset)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    if (preset != L"compact" && preset != L"comfortable" && preset != L"spacious")
    {
        return;
    }

    fence->iconSpacingPreset = preset;
    fence->inheritThemePolicy = false;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::ApplyFenceSettingsToAll(const std::wstring& sourceFenceId)
{
    FenceModel* source = FindFence(sourceFenceId);
    if (!source)
    {
        return;
    }

    for (auto& fence : m_fences)
    {
        fence.textOnlyMode = source->textOnlyMode;
        fence.inheritThemePolicy = source->inheritThemePolicy;
        fence.rollupWhenNotHovered = source->rollupWhenNotHovered;
        fence.transparentWhenNotHovered = source->transparentWhenNotHovered;
        fence.labelsOnHover = source->labelsOnHover;
        fence.iconSpacingPreset = source->iconSpacingPreset;

        if (FenceWindow* window = FindFenceWindow(fence.id))
        {
            window->UpdateModel(fence);
        }
    }

    SaveAll();
}

void FenceManager::UpdateFenceGeometry(const std::wstring& fenceId, int x, int y, int width, int height)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    fence->x = x;
    fence->y = y;
    fence->width = width;
    fence->height = height;
    SaveAll();
}

void FenceManager::Shutdown()
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

FenceModel* FenceManager::FindFence(const std::wstring& fenceId)
{
    for (auto& fence : m_fences)
    {
        if (fence.id == fenceId)
            return &fence;
    }
    return nullptr;
}

const FenceModel* FenceManager::FindFence(const std::wstring& fenceId) const
{
    for (const auto& fence : m_fences)
    {
        if (fence.id == fenceId)
            return &fence;
    }
    return nullptr;
}

FenceWindow* FenceManager::FindFenceWindow(const std::wstring& fenceId)
{
    auto it = m_windows.find(fenceId);
    if (it != m_windows.end())
        return it->second.get();
    return nullptr;
}

const FenceWindow* FenceManager::FindFenceWindow(const std::wstring& fenceId) const
{
    auto it = m_windows.find(fenceId);
    if (it != m_windows.end())
        return it->second.get();
    return nullptr;
}

const FenceModel* FenceManager::FindFenceByWindow(HWND hwnd) const
{
    if (!hwnd)
    {
        return nullptr;
    }

    for (const auto& entry : m_windows)
    {
        if (entry.second && entry.second->GetHwnd() == hwnd)
        {
            return FindFence(entry.first);
        }
    }

    return nullptr;
}

std::vector<std::wstring> FenceManager::GetAllFenceIds() const
{
    std::vector<std::wstring> ids;
    ids.reserve(m_fences.size());
    for (const auto& fence : m_fences)
    {
        ids.push_back(fence.id);
    }
    return ids;
}

void FenceManager::SetFenceExtensionRegistry(const FenceExtensionRegistry* registry)
{
    m_fenceExtensionRegistry = registry;

    bool normalizedAny = false;
    for (auto& fence : m_fences)
    {
        normalizedAny = NormalizeFenceContentProvider(fence) || normalizedAny;
    }

    if (normalizedAny)
    {
        Win32Helpers::LogInfo(L"Persisting normalized fence provider metadata after registry binding.");
        m_persistence->SaveFences(m_fences);
    }
}

void FenceManager::SetMenuContributionRegistry(const MenuContributionRegistry* registry)
{
    m_menuRegistry = registry;
}

void FenceManager::SetCommandExecutor(std::function<bool(const std::wstring&, const CommandContext&)> executor)
{
    m_commandExecutor = std::move(executor);
}

void FenceManager::SetThemePlatform(const ThemePlatform* themePlatform)
{
    m_themePlatform = themePlatform;
}

bool FenceManager::SetFenceContentSource(const std::wstring& fenceId, const std::wstring& contentSource)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return false;
    }

    if (fence->contentSource == contentSource)
    {
        return true;
    }

    fence->contentSource = contentSource;
    fence->contentState = contentSource.empty() ? L"needs_source" : L"connecting";
    fence->contentStateDetail.clear();
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
    RefreshFence(fenceId);
    return true;
}

void FenceManager::SetFenceContentState(const std::wstring& fenceId,
                                        const std::wstring& state,
                                        const std::wstring& detail)
{
    FenceModel* fence = FindFence(fenceId);
    if (!fence)
    {
        return;
    }

    const std::wstring normalizedState = state.empty() ? L"ready" : state;
    if (fence->contentState == normalizedState && fence->contentStateDetail == detail)
    {
        return;
    }

    fence->contentState = normalizedState;
    fence->contentStateDetail = detail;
    if (FenceWindow* window = FindFenceWindow(fenceId))
    {
        window->UpdateModel(*fence);
    }
    SaveAll();
}

void FenceManager::ApplyFencePresentation(const std::wstring& fenceId, const FencePresentationSettings& settings)
{
    FenceModel* sourceFence = FindFence(fenceId);
    if (!sourceFence)
    {
        return;
    }

    auto applyToFence = [&](FenceModel& fence) {
        if (settings.textOnlyMode.has_value())
        {
            fence.textOnlyMode = *settings.textOnlyMode;
        }
        if (settings.rollupWhenNotHovered.has_value())
        {
            fence.rollupWhenNotHovered = *settings.rollupWhenNotHovered;
            fence.inheritThemePolicy = false;
        }
        if (settings.transparentWhenNotHovered.has_value())
        {
            fence.transparentWhenNotHovered = *settings.transparentWhenNotHovered;
            fence.inheritThemePolicy = false;
        }
        if (settings.labelsOnHover.has_value())
        {
            fence.labelsOnHover = *settings.labelsOnHover;
            fence.inheritThemePolicy = false;
        }
        if (settings.iconSpacingPreset.has_value())
        {
            const std::wstring& preset = *settings.iconSpacingPreset;
            if (preset == L"compact" || preset == L"comfortable" || preset == L"spacious")
            {
                fence.iconSpacingPreset = preset;
                fence.inheritThemePolicy = false;
            }
        }
    };

    if (settings.applyToAll)
    {
        for (auto& fence : m_fences)
        {
            applyToFence(fence);
            if (FenceWindow* window = FindFenceWindow(fence.id))
            {
                window->UpdateModel(fence);
            }
        }
    }
    else
    {
        applyToFence(*sourceFence);
        if (FenceWindow* window = FindFenceWindow(sourceFence->id))
        {
            window->UpdateModel(*sourceFence);
        }
    }

    SaveAll();
}

std::vector<MenuContribution> FenceManager::GetMenuContributions(MenuSurface surface) const
{
    return m_menuRegistry ? m_menuRegistry->GetBySurface(surface) : std::vector<MenuContribution>{};
}

bool FenceManager::ExecuteCommand(const std::wstring& commandId, const CommandContext& context) const
{
    return m_commandExecutor ? m_commandExecutor(commandId, context) : false;
}

bool FenceManager::NormalizeFenceContentProvider(FenceModel& fence) const
{
    bool changed = false;

    if (!m_fenceExtensionRegistry)
    {
        if (fence.contentType.empty())
        {
            fence.contentType = L"file_collection";
            changed = true;
        }

        if (fence.contentPluginId.empty())
        {
            fence.contentPluginId = L"core.file_collection";
            changed = true;
        }
        return changed;
    }

    const bool supported = m_fenceExtensionRegistry->HasProvider(fence.contentType, fence.contentPluginId);
    if (supported)
    {
        return false;
    }

    const auto fallback = m_fenceExtensionRegistry->ResolveOrDefault(fence.contentType, fence.contentPluginId);
    Win32Helpers::LogError(
        L"Unsupported fence provider detected. Falling back to core provider. fenceId='" + fence.id +
        L"' contentType='" + fence.contentType + L"' pluginId='" + fence.contentPluginId + L"'");

    fence.contentType = fallback.contentType;
    fence.contentPluginId = fallback.providerId;
    return true;
}

FenceMetadata FenceManager::BuildFenceMetadata(const FenceModel& fence) const
{
    FenceMetadata meta;
    meta.id = fence.id;
    meta.title = fence.title;
    meta.backingFolderPath = fence.backingFolder;
    meta.contentType = fence.contentType;
    meta.contentPluginId = fence.contentPluginId;
    meta.contentSource = fence.contentSource;
    meta.contentState = fence.contentState;
    meta.contentStateDetail = fence.contentStateDetail;
    return meta;
}
