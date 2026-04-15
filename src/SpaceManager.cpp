#include "SpaceManager.h"
#include "SpaceWindow.h"
#include "SpaceStorage.h"
#include "Persistence.h"
#include "Win32Helpers.h"
#include "core/ThemePlatform.h"
#include "extensions/SpaceExtensionRegistry.h"
#include <algorithm>
#include <cwctype>
#include <cmath>
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

    int RectWidth(const RECT& rc)
    {
        return rc.right - rc.left;
    }

    int RectHeight(const RECT& rc)
    {
        return rc.bottom - rc.top;
    }

    POINT RectCenter(const RECT& rc)
    {
        POINT pt{};
        pt.x = rc.left + (RectWidth(rc) / 2);
        pt.y = rc.top + (RectHeight(rc) / 2);
        return pt;
    }

    bool IntersectsRectStrict(const RECT& a, const RECT& b)
    {
        RECT overlap{};
        return IntersectRect(&overlap, &a, &b) != FALSE;
    }

    std::wstring Trim(const std::wstring& text)
    {
        size_t start = 0;
        while (start < text.size() && std::iswspace(text[start]))
        {
            ++start;
        }

        size_t end = text.size();
        while (end > start && std::iswspace(text[end - 1]))
        {
            --end;
        }

        return text.substr(start, end - start);
    }

    bool HasPrefix(const std::wstring& value, const wchar_t* prefix)
    {
        if (!prefix)
        {
            return false;
        }
        const std::wstring prefixText(prefix);
        return value.size() >= prefixText.size() &&
               value.compare(0, prefixText.size(), prefixText) == 0;
    }

    struct MonitorWorkArea
    {
        RECT work{};
        bool primary = false;
    };

    BOOL CALLBACK EnumMonitorWorkAreaProc(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        if (!data)
        {
            return TRUE;
        }

        auto* areas = reinterpret_cast<std::vector<MonitorWorkArea>*>(data);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info))
        {
            return TRUE;
        }

        MonitorWorkArea area;
        area.work = info.rcWork;
        area.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        areas->push_back(area);
        return TRUE;
    }

    std::vector<MonitorWorkArea> EnumerateMonitorWorkAreas()
    {
        std::vector<MonitorWorkArea> areas;
        EnumDisplayMonitors(nullptr, nullptr, EnumMonitorWorkAreaProc, reinterpret_cast<LPARAM>(&areas));

        if (areas.empty())
        {
            RECT fallback{};
            fallback.left = 0;
            fallback.top = 0;
            fallback.right = GetSystemMetrics(SM_CXSCREEN);
            fallback.bottom = GetSystemMetrics(SM_CYSCREEN);
            areas.push_back(MonitorWorkArea{fallback, true});
        }

        return areas;
    }

    int IntersectionArea(const RECT& a, const RECT& b)
    {
        RECT overlap{};
        if (!IntersectRect(&overlap, &a, &b))
        {
            return 0;
        }
        return RectWidth(overlap) * RectHeight(overlap);
    }

    long long CenterDistanceSquared(const RECT& a, const RECT& b)
    {
        const POINT ca = RectCenter(a);
        const POINT cb = RectCenter(b);
        const long long dx = static_cast<long long>(ca.x) - static_cast<long long>(cb.x);
        const long long dy = static_cast<long long>(ca.y) - static_cast<long long>(cb.y);
        return (dx * dx) + (dy * dy);
    }

    std::optional<RECT> SelectBestWorkAreaForRect(const RECT& rc,
                                                  const std::vector<MonitorWorkArea>& areas,
                                                  bool preferPrimaryWhenNoIntersection)
    {
        if (areas.empty())
        {
            return std::nullopt;
        }

        int bestIndex = -1;
        int bestOverlap = 0;
        for (size_t i = 0; i < areas.size(); ++i)
        {
            const int overlap = IntersectionArea(rc, areas[i].work);
            if (overlap > bestOverlap)
            {
                bestOverlap = overlap;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex >= 0)
        {
            return areas[static_cast<size_t>(bestIndex)].work;
        }

        if (preferPrimaryWhenNoIntersection)
        {
            for (const auto& area : areas)
            {
                if (area.primary)
                {
                    return area.work;
                }
            }
        }

        size_t nearestIndex = 0;
        long long nearestDistance = CenterDistanceSquared(rc, areas.front().work);
        for (size_t i = 1; i < areas.size(); ++i)
        {
            const long long dist = CenterDistanceSquared(rc, areas[i].work);
            if (dist < nearestDistance)
            {
                nearestDistance = dist;
                nearestIndex = i;
            }
        }

        return areas[nearestIndex].work;
    }

    RECT ClampRectInsideRegion(const RECT& rc, const RECT& region)
    {
        RECT result = rc;
        const int w = RectWidth(rc);
        const int h = RectHeight(rc);
        const int regionLeft = static_cast<int>(region.left);
        const int regionTop = static_cast<int>(region.top);
        const int regionRight = static_cast<int>(region.right);
        const int regionBottom = static_cast<int>(region.bottom);
        const int maxLeft = (std::max)(regionLeft, regionRight - w);
        const int maxTop = (std::max)(regionTop, regionBottom - h);

        const int nextLeft = (std::max)(regionLeft, (std::min)(static_cast<int>(result.left), maxLeft));
        const int nextTop = (std::max)(regionTop, (std::min)(static_cast<int>(result.top), maxTop));
        result.left = nextLeft;
        result.top = nextTop;
        result.right = result.left + w;
        result.bottom = result.top + h;
        return result;
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

    if (ArrangeRolledUpFencesOnScreen())
    {
        PersistWithTrace(L"arrange_rolled_up_on_load", L"");
    }

    RecoverInvalidSpacesToVisibleRegions(L"load_all", true);

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

void SpaceManager::SetSettingReader(std::function<std::wstring(const std::wstring&, const std::wstring&)> reader)
{
    m_settingReader = std::move(reader);
}

void SpaceManager::DeleteSpace(const std::wstring& spaceId, const std::wstring& correlationId)
{
    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] DeleteSpace requested spaceId='" + spaceId + L"'");

    // Find and restore items to original locations
    auto space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    const auto getSetting = [this](const std::wstring& key, const std::wstring& fallback) -> std::wstring {
        return m_settingReader ? m_settingReader(key, fallback) : fallback;
    };

    const bool requireConfirm = (getSetting(L"spaces.files.confirm_delete_space", L"true") == L"true");
    if (requireConfirm)
    {
        const std::wstring message =
            L"Delete space '" + (space->title.empty() ? L"(untitled)" : space->title) +
            L"'?\n\nItems will be restored from the backing folder when possible.";
        const int decision = MessageBoxW(nullptr, message.c_str(), L"Delete Space", MB_OKCANCEL | MB_ICONQUESTION);
        if (decision != IDOK)
        {
            Win32Helpers::LogInfo(CorrelationPrefix(correlationId) + L"[SpaceManager] DeleteSpace canceled by user.");
            return;
        }
    }

    if (space)
    {
        EndLocalHoverPreview(spaceId);
        m_draggingSpaces.erase(spaceId);
        m_runtimeStates.erase(spaceId);

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

    const auto getSetting = [this](const std::wstring& key, const std::wstring& fallback) -> std::wstring {
        return m_settingReader ? m_settingReader(key, fallback) : fallback;
    };
    const bool autoRefresh = (getSetting(L"spaces.files.auto_refresh", L"true") == L"true");

    if (callbacks && callbacks->handleDrop)
    {
        const bool handled = callbacks->handleDrop(BuildSpaceMetadata(*space), paths);
        if (handled && autoRefresh)
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

    if (autoRefresh)
    {
        RefreshSpace(spaceId);
    }
    return !result.HasFailures();
}

bool SpaceManager::DeleteItem(const std::wstring& spaceId, const SpaceItem& item)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return false;
    }

    const auto getSetting = [this](const std::wstring& key, const std::wstring& fallback) -> std::wstring {
        return m_settingReader ? m_settingReader(key, fallback) : fallback;
    };

    const bool requireConfirm = (getSetting(L"spaces.files.confirm_delete_item", L"true") == L"true");
    if (requireConfirm)
    {
        const std::wstring itemLabel = item.name.empty() ? item.fullPath : item.name;
        const std::wstring message = L"Delete item '" + itemLabel + L"' from this space?";
        const int decision = MessageBoxW(nullptr, message.c_str(), L"Delete Item", MB_OKCANCEL | MB_ICONQUESTION);
        if (decision != IDOK)
        {
            return false;
        }
    }

    const SpaceContentProviderCallbacks* callbacks = m_spaceExtensionRegistry
        ? m_spaceExtensionRegistry->ResolveCallbacks(space->contentType, space->contentPluginId)
        : nullptr;
    const bool ok = (callbacks && callbacks->deleteItem)
        ? callbacks->deleteItem(BuildSpaceMetadata(*space), item)
        : m_storage->DeleteItem(space->backingFolder, item);
    const bool autoRefresh = (getSetting(L"spaces.files.auto_refresh", L"true") == L"true");
    if (ok && autoRefresh)
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

    const int previousX = space->x;
    const int previousY = space->y;

    int snappedX = x;
    int snappedY = y;
    ApplySnapAndAutoGroup(spaceId, snappedX, snappedY, width, height);

    RECT proposed{};
    proposed.left = snappedX;
    proposed.top = snappedY;
    proposed.right = snappedX + width;
    proposed.bottom = snappedY + height;

    const auto movementRegion = ResolveValidRegionForSpace(*space, proposed);
    if (movementRegion.has_value())
    {
        proposed = ClampRectInsideRegion(proposed, *movementRegion);
        snappedX = proposed.left;
        snappedY = proposed.top;
    }

    space->x = snappedX;
    space->y = snappedY;
    space->width = width;
    space->height = height;
    ClampSpaceGeometry(*space);

    const int deltaX = space->x - previousX;
    const int deltaY = space->y - previousY;
    if (!space->groupId.empty() &&
        !HasPrefix(space->groupId, L"snap:") &&
        (deltaX != 0 || deltaY != 0))
    {
        MoveConnectedStackByDelta(*space, deltaX, deltaY, correlationId);
    }

    DetachTransientSnapGroupIfSeparated(spaceId);

    SaveAll(correlationId, L"update_geometry");
}

void SpaceManager::Shutdown()
{
    SaveAll();

    for (const auto& space : m_spaces)
    {
        EndLocalHoverPreview(space.id);
    }
    m_draggingSpaces.clear();
    m_runtimeStates.clear();

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

std::wstring SpaceManager::NormalizeLayoutMode(const std::wstring& mode)
{
    const std::wstring lower = ToLower(Trim(mode));
    if (lower == L"stacked" || lower == L"contained")
    {
        return lower;
    }
    return L"free";
}

std::optional<RECT> SpaceManager::GetSpaceRect(const std::wstring& spaceId) const
{
    if (const SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        RECT rc{};
        if (GetWindowRect(window->GetHwnd(), &rc))
        {
            return rc;
        }
    }

    if (const SpaceModel* space = FindSpace(spaceId))
    {
        RECT rc{};
        rc.left = space->x;
        rc.top = space->y;
        rc.right = space->x + space->width;
        rc.bottom = space->y + space->height;
        return rc;
    }

    return std::nullopt;
}

std::optional<RECT> SpaceManager::GetParentContentBounds(const SpaceModel& space) const
{
    if (space.parentFenceId.empty())
    {
        return std::nullopt;
    }

    const auto parentRect = GetSpaceRect(space.parentFenceId);
    if (!parentRect.has_value())
    {
        return std::nullopt;
    }

    RECT content = *parentRect;
    content.left += 10;
    content.top += 34;
    content.right -= 10;
    content.bottom -= 10;
    if (content.right <= content.left || content.bottom <= content.top)
    {
        return std::nullopt;
    }

    return content;
}

std::optional<RECT> SpaceManager::GetMonitorWorkAreaForRect(const RECT& rc) const
{
    const auto areas = EnumerateMonitorWorkAreas();
    return SelectBestWorkAreaForRect(rc, areas, false);
}

bool SpaceManager::AreSpatiallyStacked(const SpaceModel& a, const SpaceModel& b) const
{
    if (!a.groupId.empty() || !b.groupId.empty() || !a.parentFenceId.empty() || !b.parentFenceId.empty())
    {
        return false;
    }

    if (!a.rollupWhenNotHovered || !b.rollupWhenNotHovered)
    {
        return false;
    }

    const auto ra = GetSpaceRect(a.id);
    const auto rb = GetSpaceRect(b.id);
    if (!ra.has_value() || !rb.has_value())
    {
        return false;
    }

    const POINT ca = RectCenter(*ra);
    const POINT cb = RectCenter(*rb);
    const int dx = std::abs(ca.x - cb.x);
    const int dy = std::abs(ca.y - cb.y);

    // Vertical-only auto-stacks: side-by-side fences should not be reflow-linked.
    const int horizontalOverlap = (std::min)(ra->right, rb->right) - (std::max)(ra->left, rb->left);
    const bool verticallyAligned = (dx <= 120) || (horizontalOverlap >= 28);
    const bool verticalDistanceOk = (dy <= 520);

    return verticallyAligned && verticalDistanceOk;
}

SpaceManager::SpaceClusterSnapshot SpaceManager::BuildClusterForSpace(const std::wstring& spaceId) const
{
    SpaceClusterSnapshot snapshot;
    const SpaceModel* anchor = FindSpace(spaceId);
    if (!anchor)
    {
        return snapshot;
    }

    if (!anchor->groupId.empty())
    {
        snapshot.clusterId = L"group:" + anchor->groupId;
        for (const auto& space : m_spaces)
        {
            if (space.groupId == anchor->groupId)
            {
                snapshot.members.push_back(space.id);
            }
        }
    }
    else if (!anchor->parentFenceId.empty())
    {
        snapshot.clusterId = L"parent:" + anchor->parentFenceId;
        for (const auto& space : m_spaces)
        {
            if (space.parentFenceId == anchor->parentFenceId)
            {
                snapshot.members.push_back(space.id);
            }
        }
        snapshot.parentBounds = GetParentContentBounds(*anchor);
    }
    else
    {
        snapshot.clusterId = L"auto:" + anchor->id;
        std::unordered_set<std::wstring> visited;
        std::vector<std::wstring> frontier;
        visited.insert(anchor->id);
        frontier.push_back(anchor->id);

        while (!frontier.empty())
        {
            const std::wstring currentId = frontier.back();
            frontier.pop_back();
            snapshot.members.push_back(currentId);

            const SpaceModel* current = FindSpace(currentId);
            if (!current)
            {
                continue;
            }

            for (const auto& space : m_spaces)
            {
                if (visited.find(space.id) != visited.end())
                {
                    continue;
                }

                if (AreSpatiallyStacked(*current, space))
                {
                    visited.insert(space.id);
                    frontier.push_back(space.id);
                }
            }
        }
    }

    if (snapshot.members.empty())
    {
        snapshot.members.push_back(anchor->id);
    }

    // Local hover reflow now only supports vertical stacks.
    snapshot.verticalAxis = true;

    if (snapshot.clusterId.rfind(L"auto:", 0) == 0)
    {
        snapshot.clusterId = snapshot.clusterId + L":" + (snapshot.verticalAxis ? L"v" : L"h");
    }

    return snapshot;
}

void SpaceManager::RestoreClusterLayout(const std::wstring& clusterId)
{
    const auto movedIt = m_reflowedMembersByCluster.find(clusterId);
    if (movedIt == m_reflowedMembersByCluster.end())
    {
        m_activePreviewByCluster.erase(clusterId);
        return;
    }

    for (const auto& memberId : movedIt->second)
    {
        const auto restIt = m_reflowRestBounds.find(memberId);
        if (restIt == m_reflowRestBounds.end())
        {
            continue;
        }

        if (SpaceWindow* memberWindow = FindSpaceWindow(memberId))
        {
            const RECT& rest = restIt->second;
            SetWindowPos(memberWindow->GetHwnd(), nullptr,
                         rest.left,
                         rest.top,
                         RectWidth(rest),
                         RectHeight(rest),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        auto runtimeIt = m_runtimeStates.find(memberId);
        if (runtimeIt != m_runtimeStates.end())
        {
            runtimeIt->second.restBounds = restIt->second;
            runtimeIt->second.visualState = FenceVisualState::Collapsed;
        }
        m_reflowRestBounds.erase(restIt);
    }

    m_reflowedMembersByCluster.erase(movedIt);
    m_activePreviewByCluster.erase(clusterId);
    Win32Helpers::LogInfo(L"[SpaceManager] Restored local stack cluster '" + clusterId + L"'.");
}

void SpaceManager::BeginLocalHoverPreview(const std::wstring& spaceId, const RECT& previewBounds)
{
    SpaceModel* anchor = FindSpace(spaceId);
    SpaceWindow* anchorWindow = FindSpaceWindow(spaceId);
    if (!anchor || !anchorWindow)
    {
        return;
    }

    const SpaceClusterSnapshot cluster = BuildClusterForSpace(spaceId);
    if (cluster.members.empty())
    {
        return;
    }

    auto activeIt = m_activePreviewByCluster.find(cluster.clusterId);
    if (activeIt != m_activePreviewByCluster.end() && activeIt->second != spaceId)
    {
        RestoreClusterLayout(cluster.clusterId);
    }

    m_activePreviewByCluster[cluster.clusterId] = spaceId;
    m_reflowedMembersByCluster[cluster.clusterId].clear();
    Win32Helpers::LogInfo(L"[SpaceManager] Hover-preview begin spaceId='" + spaceId + L"' cluster='" + cluster.clusterId +
                          L"' members=" + std::to_wstring(cluster.members.size()));

    std::optional<RECT> clusterBounds = cluster.parentBounds;
    if (!clusterBounds.has_value())
    {
        clusterBounds = GetMonitorWorkAreaForRect(previewBounds);
    }

    FenceRuntimeState& activeState = m_runtimeStates[spaceId];
    activeState.visualState = FenceVisualState::HoverPreview;
    activeState.previewBounds = previewBounds;
    activeState.zOrderPriority = 100;

    SetWindowPos(anchorWindow->GetHwnd(), HWND_TOP,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    int overlapRank = 0;
    constexpr int kBaseVerticalGap = 4;
    constexpr int kOverlapStepGap = 2;
    constexpr int kMaxSideOffset = 130;
    for (const auto& memberId : cluster.members)
    {
        if (memberId == spaceId || m_draggingSpaces.find(memberId) != m_draggingSpaces.end())
        {
            continue;
        }

        SpaceModel* sibling = FindSpace(memberId);
        SpaceWindow* siblingWindow = FindSpaceWindow(memberId);
        if (!sibling || !siblingWindow || !sibling->rollupWhenNotHovered)
        {
            continue;
        }

        const auto currentRect = GetSpaceRect(memberId);
        if (!currentRect.has_value() || !IntersectsRectStrict(previewBounds, *currentRect))
        {
            continue;
        }

        if (m_reflowRestBounds.find(memberId) == m_reflowRestBounds.end())
        {
            m_reflowRestBounds.emplace(memberId, *currentRect);
        }

        RECT shifted = *currentRect;
        const POINT c = RectCenter(*currentRect);
        const POINT pc = RectCenter(previewBounds);
        if (std::abs(c.x - pc.x) > kMaxSideOffset)
        {
            continue;
        }
        const int gap = kBaseVerticalGap + (overlapRank * kOverlapStepGap);

        bool moveDown = (c.y >= pc.y);
        if (clusterBounds.has_value())
        {
            const RECT& bounds = *clusterBounds;
            const int h = RectHeight(*currentRect);
            const int roomAbove = previewBounds.top - bounds.top;
            const int roomBelow = bounds.bottom - previewBounds.bottom;
            const bool nearTop = roomAbove < 40;
            const bool nearBottom = roomBelow < 40;

            // Top edge behavior: prefer shifting down; bottom edge behavior: prefer shifting up.
            if (nearTop && !nearBottom)
            {
                moveDown = true;
            }
            else if (nearBottom && !nearTop)
            {
                moveDown = false;
            }
            else if (roomBelow < h && roomAbove >= h)
            {
                moveDown = false;
            }
            else if (roomAbove < h && roomBelow >= h)
            {
                moveDown = true;
            }
        }

        const int h = RectHeight(*currentRect);
        if (moveDown)
        {
            shifted.top = previewBounds.bottom + gap;
            shifted.bottom = shifted.top + h;
        }
        else
        {
            shifted.bottom = previewBounds.top - gap;
            shifted.top = shifted.bottom - h;
        }

        if (clusterBounds.has_value())
        {
            const RECT& limit = *clusterBounds;
            const int w = RectWidth(shifted);
            const int shiftedHeight = RectHeight(shifted);
            shifted.left = (std::max)(limit.left, (std::min)(shifted.left, limit.right - w));
            shifted.top = (std::max)(limit.top, (std::min)(shifted.top, limit.bottom - shiftedHeight));
            shifted.right = shifted.left + w;
            shifted.bottom = shifted.top + shiftedHeight;
        }

        SetWindowPos(siblingWindow->GetHwnd(), nullptr,
                     shifted.left,
                     shifted.top,
                     RectWidth(shifted),
                     RectHeight(shifted),
                     SWP_NOZORDER | SWP_NOACTIVATE);

        Win32Helpers::LogInfo(L"[SpaceManager] Reflow sibling spaceId='" + memberId + L"' for active='" + spaceId +
                      L"' cluster='" + cluster.clusterId + L"'.");

        FenceRuntimeState& siblingState = m_runtimeStates[memberId];
        siblingState.visualState = FenceVisualState::Collapsed;
        siblingState.restBounds = m_reflowRestBounds[memberId];
        siblingState.previewBounds = shifted;
        siblingState.zOrderPriority = 10;

        m_reflowedMembersByCluster[cluster.clusterId].push_back(memberId);
        ++overlapRank;
    }
}

void SpaceManager::EndLocalHoverPreview(const std::wstring& spaceId)
{
    std::wstring clusterToRestore;
    for (const auto& entry : m_activePreviewByCluster)
    {
        if (entry.second == spaceId)
        {
            clusterToRestore = entry.first;
            break;
        }
    }

    if (clusterToRestore.empty())
    {
        return;
    }

    RestoreClusterLayout(clusterToRestore);
    Win32Helpers::LogInfo(L"[SpaceManager] Hover-preview end spaceId='" + spaceId + L"' cluster='" + clusterToRestore + L"'.");

    auto runtimeIt = m_runtimeStates.find(spaceId);
    if (runtimeIt != m_runtimeStates.end())
    {
        runtimeIt->second.visualState = FenceVisualState::Collapsed;
        runtimeIt->second.zOrderPriority = 0;
    }
}

void SpaceManager::NotifySpaceDragState(const std::wstring& spaceId, bool dragging)
{
    if (dragging)
    {
        m_draggingSpaces.insert(spaceId);
        EndLocalHoverPreview(spaceId);
        m_runtimeStates[spaceId].visualState = FenceVisualState::Dragging;
    }
    else
    {
        m_draggingSpaces.erase(spaceId);
        auto it = m_runtimeStates.find(spaceId);
        if (it != m_runtimeStates.end())
        {
            it->second.visualState = FenceVisualState::Expanded;
        }
    }
}

void SpaceManager::SetSpaceGroupId(const std::wstring& spaceId, const std::wstring& groupId)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    space->groupId = Trim(groupId);
    if (!space->groupId.empty())
    {
        space->layoutMode = L"stacked";
    }
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::SetSpaceParentFence(const std::wstring& spaceId, const std::wstring& parentFenceId)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    if (!parentFenceId.empty() && parentFenceId == spaceId)
    {
        return;
    }

    space->parentFenceId = Trim(parentFenceId);
    if (!space->parentFenceId.empty())
    {
        space->layoutMode = L"contained";
    }
    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::SetSpaceLayoutMode(const std::wstring& spaceId, const std::wstring& layoutMode)
{
    SpaceModel* space = FindSpace(spaceId);
    if (!space)
    {
        return;
    }

    space->layoutMode = NormalizeLayoutMode(layoutMode);
    if (space->layoutMode != L"contained")
    {
        space->parentFenceId.clear();
    }
    if (space->layoutMode != L"stacked")
    {
        space->groupId.clear();
    }

    if (SpaceWindow* window = FindSpaceWindow(spaceId))
    {
        window->UpdateModel(*space);
    }
    SaveAll();
}

void SpaceManager::ApplySnapAndAutoGroup(const std::wstring& movingSpaceId,
                                         int& x,
                                         int& y,
                                         int width,
                                         int height)
{
    const SnapPreviewInfo preview = ComputeSnapPreview(movingSpaceId, x, y, width, height);
    if (preview.active)
    {
        x = preview.snappedX;
        y = preview.snappedY;
    }

    SpaceModel* moving = FindSpace(movingSpaceId);
    if (!moving)
    {
        return;
    }

    if (preview.active)
    {
        std::wstring bestTargetId;
        for (const auto& candidate : m_spaces)
        {
            if (candidate.id == movingSpaceId)
            {
                continue;
            }
            const auto rect = GetSpaceRect(candidate.id);
            if (rect.has_value() &&
                rect->left == preview.targetRect.left &&
                rect->top == preview.targetRect.top &&
                rect->right == preview.targetRect.right &&
                rect->bottom == preview.targetRect.bottom)
            {
                bestTargetId = candidate.id;
                break;
            }
        }

        if (bestTargetId.empty())
        {
            return;
        }

        SpaceModel* target = FindSpace(bestTargetId);
        if (!target)
        {
            return;
        }

        std::wstring groupToUse = moving->groupId;
        if (groupToUse.empty())
        {
            groupToUse = target->groupId;
        }
        if (groupToUse.empty())
        {
            groupToUse = L"snap:" + target->id;
        }

        bool changedTarget = false;
        if (moving->groupId.empty())
        {
            moving->groupId = groupToUse;
        }
        if (target->groupId.empty())
        {
            target->groupId = groupToUse;
            changedTarget = true;
        }

        if (moving->layoutMode == L"free")
        {
            moving->layoutMode = L"stacked";
        }
        if (target->layoutMode == L"free")
        {
            target->layoutMode = L"stacked";
            changedTarget = true;
        }

        if (changedTarget)
        {
            if (SpaceWindow* targetWindow = FindSpaceWindow(target->id))
            {
                targetWindow->UpdateModel(*target);
            }
        }

        Win32Helpers::LogInfo(L"[SpaceManager] Snap grouped spaceId='" + movingSpaceId + L"' with target='" +
                              bestTargetId + L"' groupId='" + groupToUse + L"'.");
    }
}

void SpaceManager::DetachTransientSnapGroupIfSeparated(const std::wstring& movedSpaceId)
{
    SpaceModel* moved = FindSpace(movedSpaceId);
    if (!moved || moved->groupId.empty() || !HasPrefix(moved->groupId, L"snap:"))
    {
        return;
    }

    const std::wstring groupId = moved->groupId;

    auto clearTransientGroup = [&](SpaceModel& space) {
        if (space.groupId != groupId)
        {
            return;
        }

        space.groupId.clear();
        if (space.layoutMode == L"stacked" && space.parentFenceId.empty())
        {
            space.layoutMode = L"free";
        }

        if (SpaceWindow* window = FindSpaceWindow(space.id))
        {
            window->UpdateModel(space);
        }
    };

    std::vector<SpaceModel*> groupMembers;
    for (auto& space : m_spaces)
    {
        if (space.groupId == groupId)
        {
            groupMembers.push_back(&space);
        }
    }

    if (groupMembers.size() <= 1)
    {
        clearTransientGroup(*moved);
        return;
    }

    const auto movedRect = GetSpaceRect(moved->id);
    if (!movedRect.has_value())
    {
        return;
    }

    auto areVerticallyConnected = [&](const RECT& a, const RECT& b) {
        const int overlapX = (std::min)(a.right, b.right) - (std::max)(a.left, b.left);
        const int centerDx = std::abs(RectCenter(a).x - RectCenter(b).x);
        const bool alignedX = (overlapX >= 22) || (centerDx <= 110);
        if (!alignedX)
        {
            return false;
        }

        if (IntersectsRectStrict(a, b))
        {
            return true;
        }

        const int gapAB = std::abs(a.bottom - b.top);
        const int gapBA = std::abs(b.bottom - a.top);
        const int verticalGap = (std::min)(gapAB, gapBA);
        return verticalGap <= 42;
    };

    bool stillConnected = false;
    for (SpaceModel* member : groupMembers)
    {
        if (!member || member->id == moved->id)
        {
            continue;
        }

        const auto otherRect = GetSpaceRect(member->id);
        if (!otherRect.has_value())
        {
            continue;
        }

        if (areVerticallyConnected(*movedRect, *otherRect))
        {
            stillConnected = true;
            break;
        }
    }

    if (stillConnected)
    {
        return;
    }

    clearTransientGroup(*moved);

    // If one member remains in this transient snap group, clear it too.
    std::vector<SpaceModel*> remaining;
    for (auto& space : m_spaces)
    {
        if (space.groupId == groupId)
        {
            remaining.push_back(&space);
        }
    }
    if (remaining.size() <= 1 && !remaining.empty())
    {
        clearTransientGroup(*remaining.front());
    }
}

SpaceManager::SnapPreviewInfo SpaceManager::ComputeSnapPreview(const std::wstring& movingSpaceId,
                                                               int x,
                                                               int y,
                                                               int width,
                                                               int height) const
{
    SnapPreviewInfo preview;
    const SpaceModel* moving = FindSpace(movingSpaceId);
    if (!moving)
    {
        return preview;
    }

    const int kSnapDistance = 18;
    const int kEdgeSnapDistance = 22;
    const int kMinOverlap = 24;
    const int kParentSnapDistance = 16;

    RECT movingRect{};
    movingRect.left = x;
    movingRect.top = y;
    movingRect.right = x + width;
    movingRect.bottom = y + height;

    int bestDistance = kSnapDistance + 1;

    // Screen/monitor-edge snapping keeps fences on real connected desktops.
    const auto monitorAreas = EnumerateMonitorWorkAreas();
    for (const auto& area : monitorAreas)
    {
        const RECT& workArea = area.work;
        const int workLeft = static_cast<int>(workArea.left);
        const int workTop = static_cast<int>(workArea.top);
        const int workRight = static_cast<int>(workArea.right);
        const int workBottom = static_cast<int>(workArea.bottom);

        const int dLeft = std::abs(movingRect.left - workLeft);
        if (dLeft <= kEdgeSnapDistance && dLeft < bestDistance)
        {
            bestDistance = dLeft;
            preview.active = true;
            preview.snappedX = workLeft;
            preview.snappedY = y;
            preview.verticalSnap = true;
            preview.targetRect = workArea;
        }

        const int dRight = std::abs(movingRect.right - workRight);
        if (dRight <= kEdgeSnapDistance && dRight < bestDistance)
        {
            bestDistance = dRight;
            preview.active = true;
            preview.snappedX = workRight - width;
            preview.snappedY = y;
            preview.verticalSnap = true;
            preview.targetRect = workArea;
        }

        const int dTop = std::abs(movingRect.top - workTop);
        if (dTop <= kEdgeSnapDistance && dTop < bestDistance)
        {
            bestDistance = dTop;
            preview.active = true;
            preview.snappedX = x;
            preview.snappedY = workTop;
            preview.verticalSnap = false;
            preview.targetRect = workArea;
        }

        const int dBottom = std::abs(movingRect.bottom - workBottom);
        if (dBottom <= kEdgeSnapDistance && dBottom < bestDistance)
        {
            bestDistance = dBottom;
            preview.active = true;
            preview.snappedX = x;
            preview.snappedY = workBottom - height;
            preview.verticalSnap = false;
            preview.targetRect = workArea;
        }
    }

    if (!moving->parentFenceId.empty())
    {
        const auto parentBounds = GetParentContentBounds(*moving);
        if (parentBounds.has_value())
        {
            const RECT& p = *parentBounds;

            const int dLeft = std::abs(movingRect.left - p.left);
            if (dLeft <= kParentSnapDistance && dLeft < bestDistance)
            {
                bestDistance = dLeft;
                preview.active = true;
                preview.snappedX = p.left;
                preview.snappedY = y;
                preview.targetRect = p;
                preview.verticalSnap = true;
            }

            const int dRight = std::abs(movingRect.right - p.right);
            if (dRight <= kParentSnapDistance && dRight < bestDistance)
            {
                bestDistance = dRight;
                preview.active = true;
                preview.snappedX = p.right - width;
                preview.snappedY = y;
                preview.targetRect = p;
                preview.verticalSnap = true;
            }

            const int dTop = std::abs(movingRect.top - p.top);
            if (dTop <= kParentSnapDistance && dTop < bestDistance)
            {
                bestDistance = dTop;
                preview.active = true;
                preview.snappedX = x;
                preview.snappedY = p.top;
                preview.targetRect = p;
                preview.verticalSnap = false;
            }

            const int dBottom = std::abs(movingRect.bottom - p.bottom);
            if (dBottom <= kParentSnapDistance && dBottom < bestDistance)
            {
                bestDistance = dBottom;
                preview.active = true;
                preview.snappedX = x;
                preview.snappedY = p.bottom - height;
                preview.targetRect = p;
                preview.verticalSnap = false;
            }
        }
    }

    for (const auto& candidate : m_spaces)
    {
        if (candidate.id == movingSpaceId)
        {
            continue;
        }

        if (candidate.parentFenceId != moving->parentFenceId)
        {
            continue;
        }

        const auto otherRect = GetSpaceRect(candidate.id);
        if (!otherRect.has_value())
        {
            continue;
        }

        const int vertOverlap = (std::min)(movingRect.bottom, otherRect->bottom) - (std::max)(movingRect.top, otherRect->top);
        const int horzOverlap = (std::min)(movingRect.right, otherRect->right) - (std::max)(movingRect.left, otherRect->left);

        if (vertOverlap >= kMinOverlap)
        {
            const int dLeftToRight = std::abs(movingRect.left - otherRect->right);
            if (dLeftToRight < bestDistance)
            {
                bestDistance = dLeftToRight;
                preview.active = true;
                preview.snappedX = otherRect->right;
                preview.snappedY = y;
                preview.targetRect = *otherRect;
                preview.verticalSnap = true;
            }

            const int dRightToLeft = std::abs(movingRect.right - otherRect->left);
            if (dRightToLeft < bestDistance)
            {
                bestDistance = dRightToLeft;
                preview.active = true;
                preview.snappedX = otherRect->left - width;
                preview.snappedY = y;
                preview.targetRect = *otherRect;
                preview.verticalSnap = true;
            }
        }

        if (horzOverlap >= kMinOverlap)
        {
            const int dTopToBottom = std::abs(movingRect.top - otherRect->bottom);
            if (dTopToBottom < bestDistance)
            {
                bestDistance = dTopToBottom;
                preview.active = true;
                preview.snappedX = x;
                preview.snappedY = otherRect->bottom;
                preview.targetRect = *otherRect;
                preview.verticalSnap = false;
            }

            const int dBottomToTop = std::abs(movingRect.bottom - otherRect->top);
            if (dBottomToTop < bestDistance)
            {
                bestDistance = dBottomToTop;
                preview.active = true;
                preview.snappedX = x;
                preview.snappedY = otherRect->top - height;
                preview.targetRect = *otherRect;
                preview.verticalSnap = false;
            }
        }
    }

    if (!preview.active || bestDistance > (std::max)((std::max)(kSnapDistance, kEdgeSnapDistance), kParentSnapDistance))
    {
        return SnapPreviewInfo{};
    }

    return preview;
}

SpaceManager::SnapPreviewInfo SpaceManager::QuerySnapPreview(const std::wstring& movingSpaceId,
                                                             int x,
                                                             int y,
                                                             int width,
                                                             int height) const
{
    return ComputeSnapPreview(movingSpaceId, x, y, width, height);
}

void SpaceManager::ClampDragPositionToValidRegion(const std::wstring& movingSpaceId,
                                                  int width,
                                                  int height,
                                                  int& x,
                                                  int& y) const
{
    const SpaceModel* moving = FindSpace(movingSpaceId);
    if (!moving)
    {
        return;
    }

    RECT candidate{};
    candidate.left = x;
    candidate.top = y;
    candidate.right = x + width;
    candidate.bottom = y + height;

    const auto region = ResolveValidRegionForSpace(*moving, candidate);
    if (!region.has_value())
    {
        return;
    }

    const RECT clamped = ClampRectInsideRegion(candidate, *region);
    x = clamped.left;
    y = clamped.top;
}

void SpaceManager::HandleDesktopTopologyChanged(const std::wstring& reason)
{
    const DWORD now = GetTickCount();
    if (m_lastDesktopRecoveryTick != 0 && (now - m_lastDesktopRecoveryTick) < 300)
    {
        return;
    }

    m_lastDesktopRecoveryTick = now;
    RecoverInvalidSpacesToVisibleRegions(reason.empty() ? L"desktop_topology_changed" : reason, true);
}

std::optional<RECT> SpaceManager::ResolveValidRegionForSpace(const SpaceModel& space, const RECT& currentRect) const
{
    if (!space.parentFenceId.empty())
    {
        if (const auto parentBounds = GetParentContentBounds(space); parentBounds.has_value())
        {
            return parentBounds;
        }
    }

    const auto monitorAreas = EnumerateMonitorWorkAreas();
    const bool preferPrimary = monitorAreas.size() <= 1;
    return SelectBestWorkAreaForRect(currentRect, monitorAreas, preferPrimary);
}

bool SpaceManager::RecoverInvalidSpacesToVisibleRegions(const std::wstring& reason, bool persistChanges)
{
    bool anyChanged = false;

    for (auto& space : m_spaces)
    {
        RECT rect{};
        rect.left = space.x;
        rect.top = space.y;
        rect.right = space.x + space.width;
        rect.bottom = space.y + space.height;

        std::optional<RECT> region = ResolveValidRegionForSpace(space, rect);
        if (!region.has_value())
        {
            continue;
        }

        if (!space.parentFenceId.empty())
        {
            const auto parentBounds = GetParentContentBounds(space);
            if (!parentBounds.has_value())
            {
                // Parent became invalid/missing; undock and recover to desktop.
                space.parentFenceId.clear();
                if (space.layoutMode == L"contained")
                {
                    space.layoutMode = L"free";
                }
                region = ResolveValidRegionForSpace(space, rect);
                if (!region.has_value())
                {
                    continue;
                }
            }
        }

        const RECT clamped = ClampRectInsideRegion(rect, *region);
        if (clamped.left != rect.left || clamped.top != rect.top)
        {
            space.x = clamped.left;
            space.y = clamped.top;
            anyChanged = true;

            if (SpaceWindow* window = FindSpaceWindow(space.id))
            {
                SetWindowPos(window->GetHwnd(), nullptr,
                             space.x,
                             space.y,
                             space.width,
                             space.height,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                window->UpdateModel(space);
            }
        }
    }

    if (anyChanged)
    {
        Win32Helpers::LogInfo(L"[SpaceManager] Recovered invalid/offscreen fences for reason='" + reason + L"'.");
        if (persistChanges)
        {
            SaveAll(L"", L"recover_invalid_spaces");
        }
    }

    return anyChanged;
}

void SpaceManager::MoveConnectedStackByDelta(const SpaceModel& anchor,
                                             int deltaX,
                                             int deltaY,
                                             const std::wstring& correlationId)
{
    if (anchor.groupId.empty() || (deltaX == 0 && deltaY == 0))
    {
        return;
    }

    for (auto& sibling : m_spaces)
    {
        if (sibling.id == anchor.id)
        {
            continue;
        }

        if (sibling.groupId != anchor.groupId)
        {
            continue;
        }

        if (sibling.parentFenceId != anchor.parentFenceId)
        {
            continue;
        }

        int nextX = sibling.x + deltaX;
        int nextY = sibling.y + deltaY;

        RECT proposed{};
        proposed.left = nextX;
        proposed.top = nextY;
        proposed.right = nextX + sibling.width;
        proposed.bottom = nextY + sibling.height;

        const auto region = ResolveValidRegionForSpace(sibling, proposed);
        if (region.has_value())
        {
            proposed = ClampRectInsideRegion(proposed, *region);
            nextX = proposed.left;
            nextY = proposed.top;
        }

        sibling.x = nextX;
        sibling.y = nextY;

        if (SpaceWindow* siblingWindow = FindSpaceWindow(sibling.id))
        {
            SetWindowPos(siblingWindow->GetHwnd(), nullptr,
                         sibling.x,
                         sibling.y,
                         sibling.width,
                         sibling.height,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    Win32Helpers::LogInfo(CorrelationPrefix(correlationId) +
                          L"[SpaceManager] Moved connected stack group='" + anchor.groupId +
                          L"' by dx=" + std::to_wstring(deltaX) +
                          L" dy=" + std::to_wstring(deltaY));
}

bool SpaceManager::ArrangeRolledUpFencesOnScreen()
{
    struct RolledFence
    {
        std::wstring id;
        RECT rect{};
    };

    bool anyChanged = false;
    std::unordered_map<HMONITOR, std::vector<RolledFence>> byMonitor;

    for (const auto& entry : m_windows)
    {
        if (!entry.second || !entry.second->IsRolledUp())
        {
            continue;
        }

        const auto rectOpt = GetSpaceRect(entry.first);
        if (!rectOpt.has_value())
        {
            continue;
        }

        const HMONITOR monitor = MonitorFromRect(&(*rectOpt), MONITOR_DEFAULTTONEAREST);
        if (!monitor)
        {
            continue;
        }

        byMonitor[monitor].push_back(RolledFence{entry.first, *rectOpt});
    }

    for (auto& monitorEntry : byMonitor)
    {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitorEntry.first, &info))
        {
            continue;
        }

        RECT work = info.rcWork;
        constexpr int kGap = 8;

        auto& fences = monitorEntry.second;
        std::sort(fences.begin(), fences.end(), [](const RolledFence& a, const RolledFence& b) {
            if (a.rect.top != b.rect.top)
            {
                return a.rect.top < b.rect.top;
            }
            return a.rect.left < b.rect.left;
        });

        const int workLeft = static_cast<int>(work.left);
        const int workTop = static_cast<int>(work.top);
        const int workRight = static_cast<int>(work.right);
        const int workBottom = static_cast<int>(work.bottom);

        int cursorX = workLeft + kGap;
        int cursorY = workTop + kGap;
        int columnWidth = 0;

        for (const auto& fence : fences)
        {
            SpaceModel* model = FindSpace(fence.id);
            SpaceWindow* window = FindSpaceWindow(fence.id);
            if (!model || !window)
            {
                continue;
            }

            const int w = RectWidth(fence.rect);
            const int h = RectHeight(fence.rect);
            if (cursorY + h > workBottom - kGap)
            {
                cursorY = workTop + kGap;
                cursorX += columnWidth + kGap;
                columnWidth = 0;
            }

            if (cursorX + w > workRight - kGap)
            {
                cursorX = (std::max)(workLeft + kGap, workRight - w - kGap);
            }

            const int clampedY = (std::max)(workTop + kGap, (std::min)(cursorY, workBottom - h - kGap));
            const int clampedX = (std::max)(workLeft + kGap, (std::min)(cursorX, workRight - w - kGap));

            if (model->x != clampedX || model->y != clampedY)
            {
                SetWindowPos(window->GetHwnd(), nullptr, clampedX, clampedY, w, h,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                model->x = clampedX;
                model->y = clampedY;
                anyChanged = true;
            }

            cursorY = clampedY + h + kGap;
            columnWidth = (std::max)(columnWidth, w);
        }
    }

    if (anyChanged)
    {
        Win32Helpers::LogInfo(L"[SpaceManager] Repositioned rolled-up fences to avoid overlap and keep them on-screen.");
    }

    return anyChanged;
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
