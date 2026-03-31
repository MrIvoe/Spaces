#include "FenceManager.h"
#include "DesktopItemService.h"
#include "FenceRepository.h"
#include "FenceWindow.h"
#include "PathMove.h"
#include "UserIntegrityLauncher.h"
#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <shellapi.h>
#include <system_error>

namespace {

DesktopItemRef BuildDroppedItemRef(const std::wstring& sourcePath) {
    DesktopItemRef ref;
    ref.itemId = sourcePath;
    ref.sourcePath = sourcePath;
    ref.displayName = std::filesystem::path(sourcePath).filename().wstring();
    ref.source = DesktopItemSource::Desktop;

    std::error_code ec;
    ref.exists = std::filesystem::exists(sourcePath, ec);
    ref.isFolder = !ec && std::filesystem::is_directory(sourcePath, ec);
    return ref;
}

} // namespace

FenceManager::FenceManager(HINSTANCE instance, HWND desktopHost)
    : m_instance(instance), m_desktopHost(desktopHost), m_repository(std::make_unique<FenceRepository>()) {}

FenceManager::~FenceManager() = default;

void FenceManager::SetStatusCallback(StatusCallback callback) {
    m_statusCallback = std::move(callback);
}

void FenceManager::NotifyStatus(const std::wstring& title, const std::wstring& message, bool error) const {
    if (m_statusCallback) {
        m_statusCallback(title, message, error);
    }
}

bool FenceManager::Initialize() {
    if (!FenceWindow::RegisterClass(m_instance)) {
        return false;
    }

    FenceRepositoryState state;
    m_repository->Load(state);
    m_dropPolicy = state.dropPolicy;
    m_showInfoNotifications = state.showInfoNotifications;
    m_savedFences = std::move(state.fences);
    return true;
}

void FenceManager::RestoreOrCreateDefaultFence() {
    if (m_savedFences.empty()) {
        POINT pt{ 40, 40 };
        ClientToScreen(m_desktopHost, &pt);
        CreateFenceAt(pt, false);
        return;
    }

    for (const FenceData& data : m_savedFences) {
        CreateFenceFromData(data);
        m_nextId = std::max(m_nextId, data.id + 1);
    }
}

void FenceManager::CreateFenceAt(POINT screenPt, bool notifyUser) {
    FenceData data;
    data.id = m_nextId++;
    data.title = L"Fence";
    data.rect = RECT{ screenPt.x, screenPt.y, screenPt.x + 360, screenPt.y + 300 };
    data.collapsed = false;
    data.type = FenceType::Standard;
    data.backingFolder.clear();
    data.portalFolder.clear();
    CreateFenceFromData(data);
    SaveAll();

    if (notifyUser) {
        NotifyStatus(L"IVOE Fences", L"Fence created.", false);
    }
}

void FenceManager::CreatePortalFenceAt(POINT screenPt, const std::wstring& folderPath) {
    if (folderPath.empty()) {
        return;
    }

    FenceData data;
    data.id = m_nextId++;
    data.title = std::filesystem::path(folderPath).filename().wstring();
    if (data.title.empty()) {
        data.title = L"Portal";
    }
    data.rect = RECT{ screenPt.x, screenPt.y, screenPt.x + 360, screenPt.y + 300 };
    data.collapsed = false;
    data.type = FenceType::Portal;
    data.portalFolder = folderPath;
    data.backingFolder = folderPath;
    CreateFenceFromData(data);
    SaveAll();
    NotifyStatus(L"IVOE Fences", L"Portal fence created.", false);
}

void FenceManager::CreateFenceFromData(const FenceData& data) {
    auto fence = std::make_unique<FenceWindow>();

    FenceWindow::CreateParams params;
    params.instance = m_instance;
    params.parent = m_desktopHost;
    params.id = data.id;
    params.rect = data.rect;
    params.title = data.title;
    params.collapsed = data.collapsed;
    params.onChanged = [this]() { SaveAll(); };
    params.onFilesDropped = [this](int id, const std::vector<std::wstring>& paths) {
        HandleDroppedFiles(id, paths);
    };
    params.onOpenBackingFolder = [this](int id) {
        OpenFenceBackingFolder(id);
    };
    params.onDeleteFence = [this](int id) {
        DeleteFence(id);
    };
    params.canDeleteFence = [this](int id) {
        return CanDeleteFence(id);
    };
    if (data.type == FenceType::Portal) {
        params.backingFolder = !data.portalFolder.empty() ? data.portalFolder : data.backingFolder;
    }

    if (fence->Create(params)) {
        m_fenceRecords[data.id] = data;
        RefreshFenceItems(*fence, FindFenceRecord(data.id));
        m_fences.push_back(std::move(fence));
    }
}

FenceData* FenceManager::FindFenceRecord(int fenceId) {
    auto it = m_fenceRecords.find(fenceId);
    if (it == m_fenceRecords.end()) {
        return nullptr;
    }
    return &it->second;
}

const FenceData* FenceManager::FindFenceRecord(int fenceId) const {
    auto it = m_fenceRecords.find(fenceId);
    if (it == m_fenceRecords.end()) {
        return nullptr;
    }
    return &it->second;
}

std::wstring FenceManager::EnsureFenceBackingFolder(FenceWindow& fence) {
    if (!fence.GetBackingFolder().empty() && PathMove::EnsureDirectory(fence.GetBackingFolder())) {
        return fence.GetBackingFolder();
    }

    const std::wstring root = m_repository->GetFenceDataRoot().wstring();
    if (!PathMove::EnsureDirectory(root)) {
        return {};
    }

    std::wstring folder = root + L"\\Fence_" + std::to_wstring(fence.GetId());
    if (!PathMove::EnsureDirectory(folder)) {
        return {};
    }

    fence.SetBackingFolder(folder);
    return folder;
}

void FenceManager::RefreshFenceItems(FenceWindow& fence, FenceData* record) {
    std::vector<DesktopItemRef> items;
    if (record != nullptr && !record->members.empty()) {
        items = record->members;
    }

    DesktopItemService itemService;
    itemService.Initialize();

    if (record != nullptr && record->type == FenceType::Portal) {
        const std::wstring portalFolder = !record->portalFolder.empty() ? record->portalFolder : record->backingFolder;
        if (!portalFolder.empty()) {
            items = itemService.EnumerateFolderItems(portalFolder, DesktopItemSource::PortalFolder);
            record->portalFolder = portalFolder;
            record->backingFolder = portalFolder;
            record->members = items;
            fence.SetBackingFolder(portalFolder);
        }
    } else if (items.empty() && !fence.GetBackingFolder().empty()) {
        items = itemService.BuildLegacyFolderMembership(fence.GetBackingFolder());
        if (record != nullptr) {
            record->members = items;
        }
    }

    fence.SetItems(std::move(items));
}

void FenceManager::HandleDroppedFiles(int fenceId, const std::vector<std::wstring>& paths) {
    auto it = std::find_if(m_fences.begin(), m_fences.end(), [fenceId](const auto& f) {
        return f->GetId() == fenceId;
    });
    if (it == m_fences.end()) {
        return;
    }

    FenceWindow& fence = *(*it);
    FenceData* record = FindFenceRecord(fenceId);
    if (record == nullptr) {
        return;
    }

    if (record->type == FenceType::Standard) {
        bool changed = false;
        for (const std::wstring& source : paths) {
            if (source.empty()) {
                continue;
            }

            DesktopItemRef ref = BuildDroppedItemRef(source);
            auto existing = std::find_if(record->members.begin(), record->members.end(), [&ref](const DesktopItemRef& item) {
                return _wcsicmp(item.sourcePath.c_str(), ref.sourcePath.c_str()) == 0;
            });

            if (existing == record->members.end()) {
                record->members.push_back(std::move(ref));
                changed = true;
            }
        }

        if (changed) {
            RefreshFenceItems(fence, record);
            SaveAll();
        }
        return;
    }

    std::wstring destination = !record->portalFolder.empty() ? record->portalFolder : record->backingFolder;
    if (destination.empty()) {
        destination = EnsureFenceBackingFolder(fence);
    }
    if (destination.empty()) {
        return;
    }

    record->portalFolder = destination;
    record->backingFolder = destination;
    fence.SetBackingFolder(destination);

    DropPolicy policy = m_dropPolicy;
    if (policy == DropPolicy::Prompt) {
        int choice = MessageBoxW(
            fence.GetHwnd(),
            L"Drop behavior for this operation:\nYes = Move, No = Copy, Cancel = Abort",
            L"IVOE Fences Drop Policy",
            MB_ICONQUESTION | MB_YESNOCANCEL);

        if (choice == IDCANCEL) {
            return;
        }
        policy = (choice == IDYES) ? DropPolicy::Move : DropPolicy::Copy;
    }

    size_t failedCount = 0;
    size_t successCount = 0;
    for (const std::wstring& source : paths) {
        if (source.empty()) {
            continue;
        }

        bool done = false;
        if (policy == DropPolicy::Move && UserIntegrityLauncher::IsCurrentProcessElevated()) {
            done = UserIntegrityLauncher::LaunchMoveWithUserIntegrity(source, destination);
        }

        if (!done) {
            if (policy == DropPolicy::Copy) {
                done = PathMove::CopyPathToDirectory(source, destination, nullptr);
            } else {
                done = PathMove::MovePathToDirectory(source, destination, nullptr);
            }
        }

        if (!done) {
            ++failedCount;
        } else {
            ++successCount;
        }
    }

    if (record != nullptr) {
        DesktopItemService itemService;
        itemService.Initialize();
        record->members = itemService.EnumerateFolderItems(destination, DesktopItemSource::PortalFolder);
    }

    RefreshFenceItems(fence, record);
    SaveAll();

    if (failedCount > 0) {
        std::wstring action = (policy == DropPolicy::Copy) ? L"copy" : L"move";
        std::wstring message = std::to_wstring(failedCount) + L" item(s) failed to " + action + L" into the fence.";
        NotifyStatus(L"IVOE Fences", message, true);
    } else if (successCount > 0) {
        std::wstring action = (policy == DropPolicy::Copy) ? L"Copied " : L"Moved ";
        std::wstring message = action + std::to_wstring(successCount) + L" item(s) to the fence.";
        NotifyStatus(L"IVOE Fences", message, false);
    }
}

bool FenceManager::OpenFirstFenceBackingFolder() {
    if (m_fences.empty()) {
        return false;
    }

    return OpenFenceBackingFolder(m_fences.front()->GetId());
}

bool FenceManager::OpenFenceBackingFolder(int fenceId) {
    auto it = std::find_if(m_fences.begin(), m_fences.end(), [fenceId](const auto& f) {
        return f->GetId() == fenceId;
    });

    if (it == m_fences.end()) {
        return false;
    }

    FenceWindow& fence = *(*it);
    std::wstring folder = EnsureFenceBackingFolder(fence);
    if (folder.empty()) {
        NotifyStatus(L"IVOE Fences", L"Could not resolve the fence backing folder.", true);
        return false;
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        NotifyStatus(L"IVOE Fences", L"Could not open the fence backing folder.", true);
    }
    return (INT_PTR)result > 32;
}

bool FenceManager::CanDeleteFence(int fenceId) const {
    if (m_fences.size() <= 1) {
        return false;
    }

    auto it = std::find_if(m_fences.begin(), m_fences.end(), [fenceId](const auto& f) {
        return f->GetId() == fenceId;
    });

    return it != m_fences.end();
}

bool FenceManager::DeleteFence(int fenceId) {
    if (!CanDeleteFence(fenceId)) {
        NotifyStatus(L"IVOE Fences", L"At least one fence must remain.", true);
        return false;
    }

    auto it = std::find_if(m_fences.begin(), m_fences.end(), [fenceId](const auto& f) {
        return f->GetId() == fenceId;
    });

    if (it == m_fences.end()) {
        return false;
    }

    FenceWindow& fence = *(*it);
    std::wstring message = L"Delete this fence?";
    if (!fence.GetBackingFolder().empty()) {
        message += L"\n\nItems already stored in its backing folder will be left in place.";
    }
    int answer = MessageBoxW(
        fence.GetHwnd(),
        message.c_str(),
        L"IVOE Fences",
        MB_ICONWARNING | MB_OKCANCEL);

    if (answer != IDOK) {
        return false;
    }

    HWND hwnd = fence.GetHwnd();
    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }

    m_fences.erase(it);
    m_fenceRecords.erase(fenceId);
    SaveAll();
    return true;
}

void FenceManager::SetDropPolicy(DropPolicy policy) {
    m_dropPolicy = policy;
    SaveRepositoryState();
}

void FenceManager::SetShowInfoNotifications(bool enabled) {
    m_showInfoNotifications = enabled;
    SaveRepositoryState();
}

void FenceManager::SaveAll() {
    SaveRepositoryState();
}

void FenceManager::SaveRepositoryState() {
    std::vector<FenceData> data;
    data.reserve(m_fences.size());
    for (const auto& fence : m_fences) {
        FenceData snapshot = fence->ToFenceData();
        if (const FenceData* record = FindFenceRecord(snapshot.id)) {
            snapshot.type = record->type;
            snapshot.portalFolder = record->portalFolder;
            if (record->type == FenceType::Standard) {
                snapshot.backingFolder.clear();
            }
            snapshot.members = record->members;
        }
        data.push_back(std::move(snapshot));
    }

    FenceRepositoryState state;
    state.fences = std::move(data);
    state.dropPolicy = m_dropPolicy;
    state.showInfoNotifications = m_showInfoNotifications;
    state.monitorSignature = FenceRepository::BuildMonitorSignature();
    m_repository->Save(state);
}

void FenceManager::OnShellChanged() {
    RefreshAllFenceWindows();
    MaintainDesktopPlacement();
}

void FenceManager::RefreshAllFenceWindows() {
    for (const auto& fence : m_fences) {
        FenceData* record = FindFenceRecord(fence->GetId());
        RefreshFenceItems(*fence, record);
    }
    SaveRepositoryState();
}

void FenceManager::MaintainDesktopPlacement() {
    for (const auto& fence : m_fences) {
        HWND hwnd = fence->GetHwnd();
        if (!IsWindow(hwnd)) {
            continue;
        }

        if (GetParent(hwnd) != m_desktopHost) {
            SetParent(hwnd, m_desktopHost);
        }

        SetWindowPos(
            hwnd,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void FenceManager::ExitApplication() {
    SaveAll();
    PostQuitMessage(0);
}
