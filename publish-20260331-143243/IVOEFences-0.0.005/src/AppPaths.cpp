#include "AppPaths.h"

#include <windows.h>
#include <shlobj.h>

namespace {

std::filesystem::path GetModuleDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path GetKnownFolder(REFKNOWNFOLDERID folderId) {
    PWSTR folderPath = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &folderPath);
    if (FAILED(hr) || folderPath == nullptr) {
        return GetModuleDirectory();
    }

    std::filesystem::path path(folderPath);
    CoTaskMemFree(folderPath);
    return path;
}

} // namespace

std::filesystem::path AppPaths::GetAppDataRoot() {
    return GetKnownFolder(FOLDERID_LocalAppData) / "IVOEFences";
}

std::filesystem::path AppPaths::GetConfigPath() {
    return GetAppDataRoot() / "fences.json";
}

std::filesystem::path AppPaths::GetFenceDataRoot() {
    return GetAppDataRoot() / "FenceStorage";
}

std::filesystem::path AppPaths::GetLegacyIniPath() {
    return GetModuleDirectory() / "IVOEFences.ini";
}