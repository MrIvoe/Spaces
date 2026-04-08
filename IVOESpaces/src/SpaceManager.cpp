#include "SpaceManager.h"
#include "DesktopItemService.h"
#include "SpaceRepository.h"
#include "SpaceWindow.h"
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

SpaceManager::SpaceManager(HINSTANCE instance, HWND desktopHost)
    : m_instance(instance), m_desktopHost(desktopHost), m_repository(std::make_unique<SpaceRepository>()) {}

SpaceManager::~SpaceManager() = default;

void SpaceManager::SetStatusCallback(StatusCallback callback) {
    m_statusCallback = std::move(callback);
}

void SpaceManager::NotifyStatus(const std::wstring& title, const std::wstring& message, bool error) const {
    if (m_statusCallback) {
        m_statusCallback(title, message, error);
    }
}

bool SpaceManager::Initialize() {
    if (!SpaceWindow::RegisterClass(m_instance)) {
        return false;
    }

    SpaceRepositoryState state;
    m_repository->Load(state);
    m_dropPolicy = state.dropPolicy;
    m_showInfoNotifications = state.showInfoNotifications;
    m_savedSpaces = std::move(state.spaces);
    return true;
}

void SpaceManager::RestoreOrCreateDefaultSpace() {
    if (m_savedSpaces.empty()) {
        POINT pt{ 40, 40 };
        ClientToScreen(m_desktopHost, &pt);
        CreateSpaceAt(pt, false);
        return;
    }

    for (const SpaceData& data : m_savedSpaces) {
        CreateSpaceFromData(data);
        m_nextId = std::max(m_nextId, data.id + 1);
    }
}

void SpaceManager::CreateSpaceAt(POINT screenPt, bool notifyUser) {
    SpaceData data;
    data.id = m_nextId++;
    data.title = L"Space";
    data.rect = RECT{ screenPt.x, screenPt.y, screenPt.x + 360, screenPt.y + 300 };
    data.collapsed = false;
    data.type = SpaceType::Standard;
    data.backingFolder.clear();
    data.portalFolder.clear();
    CreateSpaceFromData(data);
    SaveAll();

    if (notifyUser) {
        NotifyStatus(L"IVOE Spaces", L"Space created.", false);
    }
}

void SpaceManager::CreatePortalSpaceAt(POINT screenPt, const std::wstring& folderPath) {
    if (folderPath.empty()) {
        return;
    }

    SpaceData data;
    data.id = m_nextId++;
    data.title = std::filesystem::path(folderPath).filename().wstring();
    if (data.title.empty()) {
        data.title = L"Portal";
    }
    data.rect = RECT{ screenPt.x, screenPt.y, screenPt.x + 360, screenPt.y + 300 };
    data.collapsed = false;
    data.type = SpaceType::Portal;
    data.portalFolder = folderPath;
    data.backingFolder = folderPath;
    CreateSpaceFromData(data);
    SaveAll();
    NotifyStatus(L"IVOE Spaces", L"Portal space created.", false);
}

void SpaceManager::CreateSpaceFromData(const SpaceData& data) {
    auto space = std::make_unique<SpaceWindow>();

    SpaceWindow::CreateParams params;
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
        OpenSpaceBackingFolder(id);
    };
    params.onDeleteSpace = [this](int id) {
        DeleteSpace(id);
    };
    params.canDeleteSpace = [this](int id) {
        return CanDeleteSpace(id);
    };
    if (data.type == SpaceType::Portal) {
        params.backingFolder = !data.portalFolder.empty() ? data.portalFolder : data.backingFolder;
    }

    if (space->Create(params)) {
        m_spaceRecords[data.id] = data;
        RefreshSpaceItems(*space, FindSpaceRecord(data.id));
        m_spaces.push_back(std::move(space));
    }
}

SpaceData* SpaceManager::FindSpaceRecord(int spaceId) {
    auto it = m_spaceRecords.find(spaceId);
    if (it == m_spaceRecords.end()) {
        return nullptr;
    }
    return &it->second;
}

const SpaceData* SpaceManager::FindSpaceRecord(int spaceId) const {
    auto it = m_spaceRecords.find(spaceId);
    if (it == m_spaceRecords.end()) {
        return nullptr;
    }
    return &it->second;
}

std::wstring SpaceManager::EnsureSpaceBackingFolder(SpaceWindow& space) {
    if (!space.GetBackingFolder().empty() && PathMove::EnsureDirectory(space.GetBackingFolder())) {
        return space.GetBackingFolder();
    }

    const std::wstring root = m_repository->GetSpaceDataRoot().wstring();
    if (!PathMove::EnsureDirectory(root)) {
        return {};
    }

    std::wstring folder = root + L"\\Space_" + std::to_wstring(space.GetId());
    if (!PathMove::EnsureDirectory(folder)) {
        return {};
    }

    space.SetBackingFolder(folder);
    return folder;
}

void SpaceManager::RefreshSpaceItems(SpaceWindow& space, SpaceData* record) {
    std::vector<DesktopItemRef> items;
    if (record != nullptr && !record->members.empty()) {
        items = record->members;
    }

    DesktopItemService itemService;
    itemService.Initialize();

    if (record != nullptr && record->type == SpaceType::Portal) {
        const std::wstring portalFolder = !record->portalFolder.empty() ? record->portalFolder : record->backingFolder;
        if (!portalFolder.empty()) {
            items = itemService.EnumerateFolderItems(portalFolder, DesktopItemSource::PortalFolder);
            record->portalFolder = portalFolder;
            record->backingFolder = portalFolder;
            record->members = items;
            space.SetBackingFolder(portalFolder);
        }
    } else if (items.empty() && !space.GetBackingFolder().empty()) {
        items = itemService.BuildLegacyFolderMembership(space.GetBackingFolder());
        if (record != nullptr) {
            record->members = items;
        }
    }

    space.SetItems(std::move(items));
}

void SpaceManager::HandleDroppedFiles(int spaceId, const std::vector<std::wstring>& paths) {
    auto it = std::find_if(m_spaces.begin(), m_spaces.end(), [spaceId](const auto& f) {
        return f->GetId() == spaceId;
    });
    if (it == m_spaces.end()) {
        return;
    }

    SpaceWindow& space = *(*it);
    SpaceData* record = FindSpaceRecord(spaceId);
    if (record == nullptr) {
        return;
    }

    if (record->type == SpaceType::Standard) {
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
            RefreshSpaceItems(space, record);
            SaveAll();
        }
        return;
    }

    std::wstring destination = !record->portalFolder.empty() ? record->portalFolder : record->backingFolder;
    if (destination.empty()) {
        destination = EnsureSpaceBackingFolder(space);
    }
    if (destination.empty()) {
        return;
    }

    record->portalFolder = destination;
    record->backingFolder = destination;
    space.SetBackingFolder(destination);

    DropPolicy policy = m_dropPolicy;
    if (policy == DropPolicy::Prompt) {
        int choice = MessageBoxW(
            space.GetHwnd(),
            L"Drop behavior for this operation:\nYes = Move, No = Copy, Cancel = Abort",
            L"IVOE Spaces Drop Policy",
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

    RefreshSpaceItems(space, record);
    SaveAll();

    if (failedCount > 0) {
        std::wstring action = (policy == DropPolicy::Copy) ? L"copy" : L"move";
        std::wstring message = std::to_wstring(failedCount) + L" item(s) failed to " + action + L" into the space.";
        NotifyStatus(L"IVOE Spaces", message, true);
    } else if (successCount > 0) {
        std::wstring action = (policy == DropPolicy::Copy) ? L"Copied " : L"Moved ";
        std::wstring message = action + std::to_wstring(successCount) + L" item(s) to the space.";
        NotifyStatus(L"IVOE Spaces", message, false);
    }
}

bool SpaceManager::OpenFirstSpaceBackingFolder() {
    if (m_spaces.empty()) {
        return false;
    }

    return OpenSpaceBackingFolder(m_spaces.front()->GetId());
}

bool SpaceManager::OpenSpaceBackingFolder(int spaceId) {
    auto it = std::find_if(m_spaces.begin(), m_spaces.end(), [spaceId](const auto& f) {
        return f->GetId() == spaceId;
    });

    if (it == m_spaces.end()) {
        return false;
    }

    SpaceWindow& space = *(*it);
    std::wstring folder = EnsureSpaceBackingFolder(space);
    if (folder.empty()) {
        NotifyStatus(L"IVOE Spaces", L"Could not resolve the space backing folder.", true);
        return false;
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        NotifyStatus(L"IVOE Spaces", L"Could not open the space backing folder.", true);
    }
    return (INT_PTR)result > 32;
}

bool SpaceManager::CanDeleteSpace(int spaceId) const {
    if (m_spaces.size() <= 1) {
        return false;
    }

    auto it = std::find_if(m_spaces.begin(), m_spaces.end(), [spaceId](const auto& f) {
        return f->GetId() == spaceId;
    });

    return it != m_spaces.end();
}

bool SpaceManager::DeleteSpace(int spaceId) {
    if (!CanDeleteSpace(spaceId)) {
        NotifyStatus(L"IVOE Spaces", L"At least one space must remain.", true);
        return false;
    }

    auto it = std::find_if(m_spaces.begin(), m_spaces.end(), [spaceId](const auto& f) {
        return f->GetId() == spaceId;
    });

    if (it == m_spaces.end()) {
        return false;
    }

    SpaceWindow& space = *(*it);
    std::wstring message = L"Delete this space?";
    if (!space.GetBackingFolder().empty()) {
        message += L"\n\nItems already stored in its backing folder will be left in place.";
    }
    int answer = MessageBoxW(
        space.GetHwnd(),
        message.c_str(),
        L"IVOE Spaces",
        MB_ICONWARNING | MB_OKCANCEL);

    if (answer != IDOK) {
        return false;
    }

    HWND hwnd = space.GetHwnd();
    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }

    m_spaces.erase(it);
    m_spaceRecords.erase(spaceId);
    SaveAll();
    return true;
}

void SpaceManager::SetDropPolicy(DropPolicy policy) {
    m_dropPolicy = policy;
    SaveRepositoryState();
}

void SpaceManager::SetShowInfoNotifications(bool enabled) {
    m_showInfoNotifications = enabled;
    SaveRepositoryState();
}

void SpaceManager::SaveAll() {
    SaveRepositoryState();
}

void SpaceManager::SaveRepositoryState() {
    std::vector<SpaceData> data;
    data.reserve(m_spaces.size());
    for (const auto& space : m_spaces) {
        SpaceData snapshot = space->ToSpaceData();
        if (const SpaceData* record = FindSpaceRecord(snapshot.id)) {
            snapshot.type = record->type;
            snapshot.portalFolder = record->portalFolder;
            if (record->type == SpaceType::Standard) {
                snapshot.backingFolder.clear();
            }
            snapshot.members = record->members;
        }
        data.push_back(std::move(snapshot));
    }

    SpaceRepositoryState state;
    state.spaces = std::move(data);
    state.dropPolicy = m_dropPolicy;
    state.showInfoNotifications = m_showInfoNotifications;
    state.monitorSignature = SpaceRepository::BuildMonitorSignature();
    m_repository->Save(state);
}

void SpaceManager::OnShellChanged() {
    RefreshAllSpaceWindows();
    MaintainDesktopPlacement();
}

void SpaceManager::RefreshAllSpaceWindows() {
    for (const auto& space : m_spaces) {
        SpaceData* record = FindSpaceRecord(space->GetId());
        RefreshSpaceItems(*space, record);
    }
    SaveRepositoryState();
}

void SpaceManager::MaintainDesktopPlacement() {
    for (const auto& space : m_spaces) {
        HWND hwnd = space->GetHwnd();
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

void SpaceManager::ExitApplication() {
    SaveAll();
    PostQuitMessage(0);
}
