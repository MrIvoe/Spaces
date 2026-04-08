#include "DesktopItemService.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <system_error>

namespace {

std::filesystem::path GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR folderPath = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &folderPath);
    if (FAILED(hr) || folderPath == nullptr) {
        return {};
    }

    std::filesystem::path path(folderPath);
    CoTaskMemFree(folderPath);
    return path;
}

DesktopItemRef BuildItemRefFromPath(const std::filesystem::path& path, DesktopItemSource source) {
    DesktopItemRef ref;
    ref.itemId = path.wstring();
    ref.displayName = path.filename().wstring();
    ref.sourcePath = path.wstring();
    ref.source = source;
    std::error_code ec;
    ref.isFolder = std::filesystem::is_directory(path, ec);
    ref.exists = !ec && std::filesystem::exists(path, ec);

    SHFILEINFOW fileInfo{};
    if (SHGetFileInfoW(path.c_str(), 0, &fileInfo, sizeof(fileInfo), SHGFI_SYSICONINDEX | SHGFI_SMALLICON)) {
        ref.iconIndex = fileInfo.iIcon;
    }

    return ref;
}

std::vector<DesktopItemRef> EnumeratePath(const std::filesystem::path& path, DesktopItemSource source) {
    std::vector<DesktopItemRef> result;
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error) || error) {
        return result;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path, error)) {
        if (error) {
            break;
        }
        result.push_back(BuildItemRefFromPath(entry.path(), source));
    }

    std::sort(result.begin(), result.end(), [](const DesktopItemRef& left, const DesktopItemRef& right) {
        return _wcsicmp(left.displayName.c_str(), right.displayName.c_str()) < 0;
    });

    return result;
}

} // namespace

bool DesktopItemService::Initialize() {
    m_desktopPath = GetKnownFolderPath(FOLDERID_Desktop);
    return !m_desktopPath.empty();
}

std::vector<DesktopItemRef> DesktopItemService::EnumerateDesktopItems() const {
    return EnumeratePath(m_desktopPath, DesktopItemSource::Desktop);
}

std::vector<DesktopItemRef> DesktopItemService::EnumerateFolderItems(const std::wstring& folderPath, DesktopItemSource source) const {
    return EnumeratePath(std::filesystem::path(folderPath), source);
}

std::vector<DesktopItemRef> DesktopItemService::BuildLegacyFolderMembership(const std::wstring& folderPath) const {
    return EnumerateFolderItems(folderPath, DesktopItemSource::LegacySpaceFolder);
}