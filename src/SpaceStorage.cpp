#include "SpaceStorage.h"
#include "Win32Helpers.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;
using nlohmann::json;

namespace
{
    constexpr const wchar_t* kDeletedSpaceMarkerFile = L"_deleted.marker";

    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }

        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring FromUtf8(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 0)
        {
            return {};
        }

        std::wstring result(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
        return result;
    }

    std::wstring NarrowToWide(const std::string& text)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring FormatErrorCode(const std::error_code& ec)
    {
        return L"ec=" + std::to_wstring(ec.value()) + L" msg='" + NarrowToWide(ec.message()) + L"'";
    }

    bool SaveJsonAtomic(const fs::path& targetPath, const json& value)
    {
        const fs::path tmpPath = targetPath.wstring() + L".tmp";
        std::ofstream stream(tmpPath, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            Win32Helpers::LogError(L"Failed to open temp json file for write: " + tmpPath.wstring());
            return false;
        }

        stream << value.dump(2);
        stream.flush();
        stream.close();

        if (!Win32Helpers::ReplaceFileAtomically(tmpPath, targetPath))
        {
            Win32Helpers::LogError(L"Atomic json replace failed target='" + targetPath.wstring() + L"' temp='" + tmpPath.wstring() + L"'");
            std::error_code ec;
            fs::remove(tmpPath, ec);
            return false;
        }

        return true;
    }
}

SpaceStorage::SpaceStorage()
{
    m_appRoot = Win32Helpers::GetAppDataRoot().wstring();
    m_spacesRoot = Win32Helpers::GetSpacesRoot().wstring();
    m_metadataPath = Win32Helpers::GetConfigPath().wstring();

    std::error_code ec;
    fs::create_directories(m_spacesRoot, ec);
    if (ec)
    {
        Win32Helpers::LogError(L"Failed to create spaces root: " + m_spacesRoot);
    }
}

std::wstring SpaceStorage::GetAppRoot() const
{
    return m_appRoot;
}

std::wstring SpaceStorage::GetSpacesRoot() const
{
    return m_spacesRoot;
}

std::wstring SpaceStorage::GetMetadataPath() const
{
    return m_metadataPath;
}

std::wstring SpaceStorage::EnsureSpaceFolder(const std::wstring& spaceId)
{
    const auto spacePath = fs::path(m_spacesRoot) / spaceId;
    std::error_code ec;
    fs::create_directories(spacePath, ec);
    if (ec)
    {
        Win32Helpers::LogError(L"Failed to create space folder: " + spacePath.wstring());
    }
    return spacePath.wstring();
}

std::vector<SpaceItem> SpaceStorage::ScanSpaceItems(const std::wstring& folder) const
{
    std::vector<SpaceItem> items;

    try
    {
        fs::path folderPath(folder);
        if (!fs::exists(folderPath))
            return items;

        auto origins = LoadItemOrigins(folder);

        for (const auto& entry : fs::directory_iterator(folderPath))
        {
            // Skip internal bookkeeping files.
            if (entry.path().filename() == L"_origins.json" ||
                entry.path().filename() == kDeletedSpaceMarkerFile)
                continue;

            SpaceItem item;
            item.name = entry.path().filename().wstring();
            item.fullPath = entry.path().wstring();
            item.isDirectory = fs::is_directory(entry);
            item.iconIndex = GetFileIconIndex(item.fullPath);

            // Look up original path
            auto it = origins.find(item.name);
            if (it != origins.end())
                item.originalPath = it->second;

            items.push_back(item);
        }
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"ScanSpaceItems exception in folder: " + folder + L" reason: " + NarrowToWide(ex.what()));
    }

    return items;
}

FileMoveResult SpaceStorage::MovePathsIntoSpace(const std::vector<std::wstring>& sourcePaths, const std::wstring& spaceFolder)
{
    FileMoveResult result;

    try
    {
        const fs::path destFolder(spaceFolder);
        if (!fs::exists(destFolder))
            fs::create_directories(destFolder);

        auto origins = LoadItemOrigins(spaceFolder);

        for (const auto& sourcePath : sourcePaths)
        {
            fs::path src(sourcePath);
            if (!fs::exists(src))
            {
                result.failed.push_back({src, L"Source path does not exist"});
                Win32Helpers::LogError(L"Move skipped (missing source): src='" + src.wstring() + L"' destFolder='" + destFolder.wstring() + L"'");
                continue;
            }

            auto filename = src.filename();
            auto dest = destFolder / filename;

            // Handle duplicates
            int counter = 2;
            auto stem = filename.stem();
            auto ext = filename.extension();
            while (fs::exists(dest) && counter < 10000)
            {
                auto newName = stem.wstring() + L" (" + std::to_wstring(counter) + L")" + ext.wstring();
                dest = destFolder / newName;
                ++counter;
            }

            // Try rename first (same volume)
            std::error_code ec;
            fs::rename(src, dest, ec);
            if (!ec)
            {
                origins[dest.filename().wstring()] = src.wstring();
                result.moved.push_back(dest);
                Win32Helpers::LogInfo(L"Move success via rename: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
                continue;
            }

            const std::error_code renameEc = ec;

            // Fall back to copy+delete
            if (fs::is_directory(src))
            {
                fs::copy(src, dest, fs::copy_options::recursive, ec);
                if (!ec)
                {
                    fs::remove_all(src, ec);
                    if (!ec)
                    {
                        origins[dest.filename().wstring()] = src.wstring();
                        result.moved.push_back(dest);
                        Win32Helpers::LogInfo(L"Move success via copy+remove_all: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
                        continue;
                    }
                    Win32Helpers::LogError(
                        L"Move remove source dir failed after copy: src='" + src.wstring() +
                        L"' dest='" + dest.wstring() + L"' " + FormatErrorCode(ec));
                }
                else
                {
                    Win32Helpers::LogError(
                        L"Move copy dir failed: src='" + src.wstring() +
                        L"' dest='" + dest.wstring() + L"' rename=" + FormatErrorCode(renameEc) +
                        L" copy=" + FormatErrorCode(ec));
                }
            }
            else
            {
                fs::copy_file(src, dest, fs::copy_options::none, ec);
                if (!ec)
                {
                    fs::remove(src, ec);
                    if (!ec)
                    {
                        origins[dest.filename().wstring()] = src.wstring();
                        result.moved.push_back(dest);
                        Win32Helpers::LogInfo(L"Move success via copy+remove: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
                        continue;
                    }
                    Win32Helpers::LogError(
                        L"Move remove source file failed after copy: src='" + src.wstring() +
                        L"' dest='" + dest.wstring() + L"' " + FormatErrorCode(ec));
                }
                else
                {
                    Win32Helpers::LogError(
                        L"Move copy file failed: src='" + src.wstring() +
                        L"' dest='" + dest.wstring() + L"' rename=" + FormatErrorCode(renameEc) +
                        L" copy=" + FormatErrorCode(ec));
                }
            }

            result.failed.push_back({src, L"Move failed after rename and copy fallback"});
            Win32Helpers::LogError(L"Move failed: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
        }

        // Save origins for later restoration
        SaveItemOrigins(spaceFolder, origins);
        return result;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"MovePathsIntoSpace exception for folder: " + spaceFolder + L" reason: " + NarrowToWide(ex.what()));
        result.failed.push_back({fs::path(spaceFolder), L"Unhandled exception during move"});
        return result;
    }
}

std::wstring SpaceStorage::GetOriginsPath(const std::wstring& spaceFolder) const
{
    return spaceFolder + L"\\_origins.json";
}

void SpaceStorage::SaveItemOrigins(const std::wstring& spaceFolder, const std::map<std::wstring, std::wstring>& origins)
{
    try
    {
        json root = json::object();
        for (const auto& [filename, originalPath] : origins)
        {
            root[ToUtf8(filename)] = ToUtf8(originalPath);
        }

        SaveJsonAtomic(GetOriginsPath(spaceFolder), root);
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SaveItemOrigins exception for space folder: " + spaceFolder + L" reason: " + NarrowToWide(ex.what()));
    }
}

std::map<std::wstring, std::wstring> SpaceStorage::LoadItemOrigins(const std::wstring& spaceFolder) const
{
    std::map<std::wstring, std::wstring> origins;

    try
    {
        auto originsFile = GetOriginsPath(spaceFolder);
        if (!fs::exists(originsFile))
            return origins;

        std::ifstream file(originsFile, std::ios::binary);
        if (!file.is_open())
        {
            return origins;
        }

        json root = json::parse(file, nullptr, false);
        if (!root.is_object())
        {
            Win32Helpers::LogError(L"Origins file is malformed json: " + originsFile);
            return origins;
        }

        for (auto it = root.begin(); it != root.end(); ++it)
        {
            if (!it.value().is_string())
            {
                continue;
            }

            const std::string filename = it.key();
            const std::string originalPath = it.value().get<std::string>();
            origins[FromUtf8(filename)] = FromUtf8(originalPath);
        }

        return origins;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"LoadItemOrigins exception for space folder: " + spaceFolder + L" reason: " + NarrowToWide(ex.what()));
        return origins;
    }
}

bool SpaceStorage::RestoreItemToOrigin(const std::wstring& spaceFolder,
                                       const SpaceItem& item,
                                       std::wstring* failureReason,
                                       std::filesystem::path* restoredDestination)
{
    try
    {
        if (item.originalPath.empty())
        {
            if (failureReason)
            {
                *failureReason = L"Item has no original path metadata";
            }
            return false;
        }

        fs::path src(item.fullPath);
        fs::path dest(item.originalPath);

        if (!fs::exists(src))
        {
            if (failureReason)
            {
                *failureReason = L"Source path does not exist during restore";
            }
            return false;
        }

        // Ensure destination directory exists
        fs::path destDir = dest.parent_path();
        if (!fs::exists(destDir))
            fs::create_directories(destDir);

        dest = GenerateNonConflictingPath(dest);
        Win32Helpers::LogInfo(L"Restoring item to: " + dest.wstring());

        std::error_code ec;

        // Try rename first
        fs::rename(src, dest, ec);
        if (!ec)
        {
            if (restoredDestination)
            {
                *restoredDestination = dest;
            }
            auto origins = LoadItemOrigins(spaceFolder);
            origins.erase(fs::path(item.fullPath).filename().wstring());
            SaveItemOrigins(spaceFolder, origins);
            Win32Helpers::LogInfo(L"Restore success via rename: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
            return true;
        }

        const std::error_code renameEc = ec;

        // Fall back to copy+delete
        if (fs::is_directory(src))
        {
            fs::copy(src, dest, fs::copy_options::recursive, ec);
            if (!ec)
            {
                fs::remove_all(src, ec);
                if (!ec)
                {
                    if (restoredDestination)
                    {
                        *restoredDestination = dest;
                    }
                    auto origins = LoadItemOrigins(spaceFolder);
                    origins.erase(fs::path(item.fullPath).filename().wstring());
                    SaveItemOrigins(spaceFolder, origins);
                    Win32Helpers::LogInfo(L"Restore success via copy+remove_all: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
                    return true;
                }

                if (failureReason)
                {
                    *failureReason = L"Copied directory to destination but failed to remove source";
                }
            }

            if (failureReason && failureReason->empty())
            {
                *failureReason = L"Directory restore failed after rename and copy fallback";
            }

            Win32Helpers::LogError(
                L"Restore directory copy/remove failed: src='" + src.wstring() +
                L"' dest='" + dest.wstring() + L"' rename=" + FormatErrorCode(renameEc) +
                L" copy/remove=" + FormatErrorCode(ec));
        }
        else
        {
            fs::copy_file(src, dest, fs::copy_options::none, ec);
            if (!ec)
            {
                fs::remove(src, ec);
                if (!ec)
                {
                    if (restoredDestination)
                    {
                        *restoredDestination = dest;
                    }
                    auto origins = LoadItemOrigins(spaceFolder);
                    origins.erase(fs::path(item.fullPath).filename().wstring());
                    SaveItemOrigins(spaceFolder, origins);
                    Win32Helpers::LogInfo(L"Restore success via copy+remove: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
                    return true;
                }

                if (failureReason)
                {
                    *failureReason = L"Copied file to destination but failed to remove source";
                }
            }

            if (failureReason && failureReason->empty())
            {
                *failureReason = L"File restore failed after rename and copy fallback";
            }

            Win32Helpers::LogError(
                L"Restore file copy/remove failed: src='" + src.wstring() +
                L"' dest='" + dest.wstring() + L"' rename=" + FormatErrorCode(renameEc) +
                L" copy/remove=" + FormatErrorCode(ec));
        }

        if (failureReason && failureReason->empty())
        {
            *failureReason = L"Restore failed";
        }

        Win32Helpers::LogError(L"Restore failed: src='" + src.wstring() + L"' dest='" + dest.wstring() + L"'");
        return false;
    }
    catch (const std::exception& ex)
    {
        if (failureReason)
        {
            *failureReason = L"Unhandled restore exception";
        }
        Win32Helpers::LogError(L"RestoreItemToOrigin exception for item: " + item.fullPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

RestoreResult SpaceStorage::RestoreAllItems(const std::wstring& spaceFolder)
{
    RestoreResult result;

    try
    {
        auto items = ScanSpaceItems(spaceFolder);
        for (const auto& item : items)
        {
            std::wstring reason;
            std::filesystem::path restoredPath;
            if (RestoreItemToOrigin(spaceFolder, item, &reason, &restoredPath))
            {
                ++result.restoredCount;
                if (!restoredPath.empty())
                {
                    result.restoredItems.push_back(restoredPath);
                }
            }
            else
            {
                ++result.failedCount;
                if (reason.empty())
                {
                    reason = L"Restore failed";
                }
                result.failedItems.push_back({fs::path(item.fullPath), reason});
            }
        }
        return result;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"RestoreAllItems exception for space folder: " + spaceFolder + L" reason: " + NarrowToWide(ex.what()));
        ++result.failedCount;
        result.failedItems.push_back({fs::path(spaceFolder), L"Unhandled restore-all exception"});
        return result;
    }
}

bool SpaceStorage::DeleteItem(const std::wstring& spaceFolder, const SpaceItem& item)
{
    if (!item.originalPath.empty())
    {
        return RestoreItemToOrigin(spaceFolder, item);
    }

    std::error_code ec;
    if (fs::is_directory(item.fullPath, ec))
    {
        fs::remove_all(item.fullPath, ec);
    }
    else
    {
        fs::remove(item.fullPath, ec);
    }

    auto origins = LoadItemOrigins(spaceFolder);
    origins.erase(fs::path(item.fullPath).filename().wstring());
    SaveItemOrigins(spaceFolder, origins);

    if (ec)
    {
        Win32Helpers::LogError(L"Delete item failed path='" + item.fullPath + L"' " + FormatErrorCode(ec));
        return false;
    }

    return true;
}

bool SpaceStorage::DeleteSpaceFolderIfEmpty(const std::wstring& spaceFolder)
{
    std::error_code ec;
    const fs::path folderPath(spaceFolder);
    const fs::path originsPath = GetOriginsPath(spaceFolder);

    fs::remove(originsPath, ec);
    ec.clear();

    if (!fs::exists(folderPath, ec))
    {
        return true;
    }

    const bool isEmpty = fs::is_empty(folderPath, ec);
    if (ec)
    {
        Win32Helpers::LogError(L"Failed checking if space folder is empty: " + folderPath.wstring());
        return false;
    }

    if (!isEmpty)
    {
        Win32Helpers::LogInfo(L"Space folder not empty, leaving in place: " + folderPath.wstring());
        return false;
    }

    fs::remove(folderPath, ec);
    if (ec)
    {
        Win32Helpers::LogError(L"Failed removing space folder: " + folderPath.wstring());
        return false;
    }

    return true;
}

bool SpaceStorage::MarkSpaceDeleted(const std::wstring& spaceFolder)
{
    try
    {
        const fs::path folderPath(spaceFolder);
        std::error_code ec;
        fs::create_directories(folderPath, ec);
        if (ec)
        {
            Win32Helpers::LogError(L"MarkSpaceDeleted create_directories failed: folder='" + spaceFolder +
                                   L"' " + FormatErrorCode(ec));
            return false;
        }

        const fs::path markerPath = folderPath / kDeletedSpaceMarkerFile;
        std::ofstream marker(markerPath, std::ios::binary | std::ios::trunc);
        if (!marker.is_open())
        {
            Win32Helpers::LogError(L"MarkSpaceDeleted failed opening marker file: " + markerPath.wstring());
            return false;
        }

        marker << "deleted";
        marker.flush();
        marker.close();
        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"MarkSpaceDeleted exception for folder: " + spaceFolder +
                               L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool SpaceStorage::IsSpaceDeletedMarked(const std::wstring& spaceFolder) const
{
    try
    {
        const fs::path markerPath = fs::path(spaceFolder) / kDeletedSpaceMarkerFile;
        return fs::exists(markerPath);
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"IsSpaceDeletedMarked exception for folder: " + spaceFolder +
                               L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool SpaceStorage::ClearSpaceDeletedMarker(const std::wstring& spaceFolder)
{
    try
    {
        const fs::path markerPath = fs::path(spaceFolder) / kDeletedSpaceMarkerFile;
        std::error_code ec;
        if (!fs::exists(markerPath))
        {
            return true;
        }

        fs::remove(markerPath, ec);
        if (ec)
        {
            Win32Helpers::LogError(L"ClearSpaceDeletedMarker failed: marker='" + markerPath.wstring() +
                                   L"' " + FormatErrorCode(ec));
            return false;
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"ClearSpaceDeletedMarker exception for folder: " + spaceFolder +
                               L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

int SpaceStorage::GetFileIconIndex(const std::wstring& filePath)
{
    try
    {
        SHFILEINFOW sfi{};
        UINT flags = SHGFI_SYSICONINDEX | SHGFI_LARGEICON;
        DWORD attrs = 0;

        if (!fs::exists(filePath))
        {
            flags |= SHGFI_USEFILEATTRIBUTES;
            attrs = FILE_ATTRIBUTE_NORMAL;
        }

        HIMAGELIST hImageList = reinterpret_cast<HIMAGELIST>(
            SHGetFileInfoW(filePath.c_str(), attrs, &sfi, sizeof(sfi), flags));

        // Icon index should be valid when we get the image list handle
        if (hImageList != nullptr)
            return sfi.iIcon;

        return 0;  // Default icon index
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"GetFileIconIndex exception for path: " + filePath + L" reason: " + NarrowToWide(ex.what()));
        return 0;
    }
}

std::filesystem::path SpaceStorage::GenerateNonConflictingPath(const std::filesystem::path& target) const
{
    if (!fs::exists(target))
    {
        return target;
    }

    const std::filesystem::path parent = target.parent_path();
    const std::wstring stem = target.stem().wstring();
    const std::wstring ext = target.extension().wstring();

    for (int counter = 1; counter < 10000; ++counter)
    {
        const std::wstring candidateName = stem + L" (restored " + std::to_wstring(counter) + L")" + ext;
        const std::filesystem::path candidate = parent / candidateName;
        if (!fs::exists(candidate))
        {
            return candidate;
        }
    }

    return parent / (stem + L" (restored)" + ext);
}
