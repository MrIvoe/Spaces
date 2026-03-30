#include "FenceStorage.h"
#include "Win32Helpers.h"
#include <filesystem>
#include <windows.h>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

FenceStorage::FenceStorage()
{
    m_appRoot = Win32Helpers::GetLocalAppDataPath() + L"\\SimpleFences";
    m_fencesRoot = m_appRoot + L"\\Fences";
    m_metadataPath = m_appRoot + L"\\config.json";

    fs::create_directories(m_fencesRoot);
}

std::wstring FenceStorage::GetAppRoot() const
{
    return m_appRoot;
}

std::wstring FenceStorage::GetFencesRoot() const
{
    return m_fencesRoot;
}

std::wstring FenceStorage::GetMetadataPath() const
{
    return m_metadataPath;
}

std::wstring FenceStorage::EnsureFenceFolder(const std::wstring& fenceId)
{
    auto fencePath = fs::path(m_fencesRoot) / fenceId;
    fs::create_directories(fencePath);
    return fencePath.wstring();
}

std::vector<FenceItem> FenceStorage::ScanFenceItems(const std::wstring& folder) const
{
    std::vector<FenceItem> items;

    try
    {
        fs::path folderPath(folder);
        if (!fs::exists(folderPath))
            return items;

        auto origins = LoadItemOrigins(folder);

        for (const auto& entry : fs::directory_iterator(folderPath))
        {
            // Skip the origins file itself
            if (entry.path().filename() == L"_origins.json")
                continue;

            FenceItem item;
            item.name = entry.path().filename().wstring();
            item.fullPath = entry.path().wstring();
            item.isDirectory = fs::is_directory(entry);

            // Look up original path
            auto it = origins.find(item.name);
            if (it != origins.end())
                item.originalPath = it->second;

            items.push_back(item);
        }
    }
    catch (const std::exception&)
    {
    }

    return items;
}

bool FenceStorage::MovePathsIntoFence(const std::vector<std::wstring>& sourcePaths, const std::wstring& fenceFolder)
{
    try
    {
        fs::path destFolder(fenceFolder);
        if (!fs::exists(destFolder))
            fs::create_directories(destFolder);

        auto origins = LoadItemOrigins(fenceFolder);

        for (const auto& sourcePath : sourcePaths)
        {
            fs::path src(sourcePath);
            if (!fs::exists(src))
                continue;

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

            // Store original path in origins map
            origins[dest.filename().wstring()] = src.wstring();

            // Try rename first (same volume)
            std::error_code ec;
            fs::rename(src, dest, ec);
            if (!ec)
                continue;

            // Fall back to copy+delete
            if (fs::is_directory(src))
            {
                fs::copy(src, dest, fs::copy_options::recursive, ec);
                if (!ec)
                    fs::remove_all(src, ec);
            }
            else
            {
                fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
                if (!ec)
                    fs::remove(src, ec);
            }
        }

        // Save origins for later restoration
        SaveItemOrigins(fenceFolder, origins);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::wstring FenceStorage::GetOriginsPath(const std::wstring& fenceFolder) const
{
    return fenceFolder + L"\\_origins.json";
}

void FenceStorage::SaveItemOrigins(const std::wstring& fenceFolder, const std::map<std::wstring, std::wstring>& origins)
{
    try
    {
        auto originsFile = GetOriginsPath(fenceFolder);
        std::wofstream file(originsFile);
        if (!file.is_open())
            return;

        for (const auto& [filename, originalPath] : origins)
        {
            // Simple JSON: each line is a filename:originalpath mapping
            file << L"\"" << filename << L"\":\"" << originalPath << L"\"\n";
        }

        file.close();
    }
    catch (const std::exception&)
    {
    }
}

std::map<std::wstring, std::wstring> FenceStorage::LoadItemOrigins(const std::wstring& fenceFolder) const
{
    std::map<std::wstring, std::wstring> origins;

    try
    {
        auto originsFile = GetOriginsPath(fenceFolder);
        if (!fs::exists(originsFile))
            return origins;

        std::wifstream file(originsFile);
        if (!file.is_open())
            return origins;

        std::wstring line;
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            // Parse: "filename":"originalpath"
            size_t colonPos = line.find(L"\":");
            if (colonPos == std::wstring::npos)
                continue;

            size_t start1 = line.find(L"\"");
            size_t end1 = line.find(L"\"", start1 + 1);
            if (end1 == std::wstring::npos)
                continue;

            std::wstring filename = line.substr(start1 + 1, end1 - start1 - 1);

            size_t start2 = line.find(L"\"", end1 + 2);
            size_t end2 = line.rfind(L"\"");
            if (start2 == std::wstring::npos || end2 == std::wstring::npos || end2 <= start2)
                continue;

            std::wstring originalPath = line.substr(start2 + 1, end2 - start2 - 1);
            origins[filename] = originalPath;
        }

        file.close();
        return origins;
    }
    catch (const std::exception&)
    {
        return origins;
    }
}

bool FenceStorage::RestoreItemToOrigin(const std::wstring& fenceFolder, const FenceItem& item)
{
    try
    {
        if (item.originalPath.empty())
            return false;

        fs::path src(item.fullPath);
        fs::path dest(item.originalPath);

        if (!fs::exists(src))
            return false;

        // Ensure destination directory exists
        fs::path destDir = dest.parent_path();
        if (!fs::exists(destDir))
            fs::create_directories(destDir);

        std::error_code ec;

        // Try rename first
        fs::rename(src, dest, ec);
        if (!ec)
        {
            // Update origins - remove this entry
            auto origins = LoadItemOrigins(fenceFolder);
            origins.erase(item.name);
            SaveItemOrigins(fenceFolder, origins);
            return true;
        }

        // Fall back to copy+delete
        if (fs::is_directory(src))
        {
            fs::copy(src, dest, fs::copy_options::recursive, ec);
            if (!ec)
            {
                fs::remove_all(src, ec);
                auto origins = LoadItemOrigins(fenceFolder);
                origins.erase(item.name);
                SaveItemOrigins(fenceFolder, origins);
                return true;
            }
        }
        else
        {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
            if (!ec)
            {
                fs::remove(src, ec);
                auto origins = LoadItemOrigins(fenceFolder);
                origins.erase(item.name);
                SaveItemOrigins(fenceFolder, origins);
                return true;
            }
        }

        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool FenceStorage::RestoreAllItems(const std::wstring& fenceFolder)
{
    try
    {
        auto items = ScanFenceItems(fenceFolder);
        for (const auto& item : items)
        {
            RestoreItemToOrigin(fenceFolder, item);
        }
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
