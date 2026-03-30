#include "Persistence.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

Persistence::Persistence(const std::wstring& metadataPath)
    : m_metadataPath(metadataPath)
{
    EnsureDirectory();
}

bool Persistence::EnsureDirectory()
{
    try
    {
        auto dir = fs::path(m_metadataPath).parent_path();
        fs::create_directories(dir);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool Persistence::LoadFences(std::vector<FenceModel>& fences)
{
    try
    {
        if (!fs::exists(m_metadataPath))
            return true;

        std::wifstream file(m_metadataPath);
        if (!file.is_open())
            return false;

        // Simple JSON parsing for fence data
        // Format: each fence on one line as JSON object
        std::wstring line;
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            // Very basic JSON parsing
            FenceModel fence;
            fence.width = 320;
            fence.height = 240;

            // Extract id
            size_t idPos = line.find(L"\"id\":\"");
            if (idPos != std::wstring::npos)
            {
                size_t start = idPos + 6;
                size_t end = line.find(L"\"", start);
                if (end != std::wstring::npos)
                    fence.id = line.substr(start, end - start);
            }

            // Extract title
            size_t titlePos = line.find(L"\"title\":\"");
            if (titlePos != std::wstring::npos)
            {
                size_t start = titlePos + 9;
                size_t end = line.find(L"\"", start);
                if (end != std::wstring::npos)
                    fence.title = line.substr(start, end - start);
            }

            // Extract x
            size_t xPos = line.find(L"\"x\":");
            if (xPos != std::wstring::npos)
            {
                fence.x = _wtoi(line.substr(xPos + 4).c_str());
            }

            // Extract y
            size_t yPos = line.find(L"\"y\":");
            if (yPos != std::wstring::npos)
            {
                fence.y = _wtoi(line.substr(yPos + 4).c_str());
            }

            // Extract width
            size_t widthPos = line.find(L"\"width\":");
            if (widthPos != std::wstring::npos)
            {
                fence.width = _wtoi(line.substr(widthPos + 8).c_str());
            }

            // Extract height
            size_t heightPos = line.find(L"\"height\":");
            if (heightPos != std::wstring::npos)
            {
                fence.height = _wtoi(line.substr(heightPos + 9).c_str());
            }

            // Extract backingFolder
            size_t folderPos = line.find(L"\"backingFolder\":\"");
            if (folderPos != std::wstring::npos)
            {
                size_t start = folderPos + 17;
                size_t end = line.find(L"\"", start);
                if (end != std::wstring::npos)
                    fence.backingFolder = line.substr(start, end - start);
            }

            if (!fence.id.empty())
                fences.push_back(fence);
        }

        file.close();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool Persistence::SaveFences(const std::vector<FenceModel>& fences)
{
    try
    {
        std::wofstream file(m_metadataPath);
        if (!file.is_open())
            return false;

        for (const auto& fence : fences)
        {
            file << L"{\"id\":\"" << fence.id << L"\",";
            file << L"\"title\":\"" << fence.title << L"\",";
            file << L"\"x\":" << fence.x << L",";
            file << L"\"y\":" << fence.y << L",";
            file << L"\"width\":" << fence.width << L",";
            file << L"\"height\":" << fence.height << L",";
            file << L"\"backingFolder\":\"" << fence.backingFolder << L"\"}\n";
        }

        file.close();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool Persistence::SaveFence(const FenceModel& fence)
{
    try
    {
        std::wofstream file(m_metadataPath, std::ios::app);
        if (!file.is_open())
            return false;

        file << L"{\"id\":\"" << fence.id << L"\",";
        file << L"\"title\":\"" << fence.title << L"\",";
        file << L"\"x\":" << fence.x << L",";
        file << L"\"y\":" << fence.y << L",";
        file << L"\"width\":" << fence.width << L",";
        file << L"\"height\":" << fence.height << L",";
        file << L"\"backingFolder\":\"" << fence.backingFolder << L"\"}\n";

        file.close();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
