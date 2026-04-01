#include "FenceManager.h"
#include "FenceWindow.h"
#include "FenceStorage.h"
#include "Persistence.h"
#include "Win32Helpers.h"
#include "extensions/FenceExtensionRegistry.h"
#include <algorithm>
#include <objbase.h>
#include <ole2.h>
#include <sstream>

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

        auto window = std::make_unique<FenceWindow>(this, fence);
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
    FenceModel fence;
    fence.id = GenerateFenceId();
    fence.title = title;
    fence.x = x;
    fence.y = y;
    fence.width = 320;
    fence.height = 240;
    fence.backingFolder = m_storage->EnsureFenceFolder(fence.id);
    fence.contentType = L"file_collection";
    fence.contentPluginId = L"core.file_collection";

    NormalizeFenceContentProvider(fence);

    m_fences.push_back(fence);

    auto window = std::make_unique<FenceWindow>(this, fence);
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

    auto items = m_storage->ScanFenceItems(fence->backingFolder);

    auto window = FindFenceWindow(fenceId);
    if (window)
        window->SetItems(items);
}

void FenceManager::RefreshAll()
{
    for (auto& fence : m_fences)
    {
        auto items = m_storage->ScanFenceItems(fence.backingFolder);
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

    FileMoveResult result = m_storage->MovePathsIntoFence(paths, fence->backingFolder);
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

    const bool ok = m_storage->DeleteItem(fence->backingFolder, item);
    if (ok)
    {
        RefreshFence(fenceId);
    }
    return ok;
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

FenceWindow* FenceManager::FindFenceWindow(const std::wstring& fenceId)
{
    auto it = m_windows.find(fenceId);
    if (it != m_windows.end())
        return it->second.get();
    return nullptr;
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
