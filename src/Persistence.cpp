#include "Persistence.h"
#include "Win32Helpers.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;
using nlohmann::json;

namespace
{
    std::wstring NarrowToWide(const std::string& text)
    {
        return std::wstring(text.begin(), text.end());
    }

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
}

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
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"EnsureDirectory failed for metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool Persistence::LoadFences(std::vector<FenceModel>& fences)
{
    try
    {
        fences.clear();

        if (!fs::exists(m_metadataPath))
            return true;

        std::ifstream file(m_metadataPath, std::ios::binary);
        if (!file.is_open())
        {
            Win32Helpers::LogError(L"Failed to open metadata file for read: " + m_metadataPath);
            return false;
        }

        json root = json::parse(file, nullptr, false);
        if (!root.is_object())
        {
            Win32Helpers::LogError(L"Metadata file is malformed json, ignoring data: " + m_metadataPath);
            return false;
        }

        if (!root.contains("fences") || !root["fences"].is_array())
        {
            return true;
        }

        for (const auto& item : root["fences"])
        {
            if (!item.is_object())
            {
                continue;
            }

            FenceModel fence;
            fence.id = FromUtf8(item.value("id", std::string{}));
            if (fence.id.empty())
            {
                continue;
            }

            fence.title = FromUtf8(item.value("title", std::string{"Fence"}));
            fence.x = item.value("x", 100);
            fence.y = item.value("y", 100);
            fence.width = item.value("width", 320);
            fence.height = item.value("height", 240);
            fence.backingFolder = FromUtf8(item.value("backingFolder", std::string{}));
            fence.contentType = FromUtf8(item.value("contentType", std::string{"file_collection"}));
            fence.contentPluginId = FromUtf8(item.value("contentPluginId", std::string{"core.file_collection"}));
            fence.appearanceProfileId = FromUtf8(item.value("appearanceProfileId", std::string{}));
            fence.widgetLayoutId = FromUtf8(item.value("widgetLayoutId", std::string{}));

            if (fence.contentType.empty())
            {
                fence.contentType = L"file_collection";
            }

            if (fence.contentPluginId.empty())
            {
                fence.contentPluginId = L"core.file_collection";
            }
            fences.push_back(std::move(fence));
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"LoadFences exception for metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool Persistence::SaveFences(const std::vector<FenceModel>& fences)
{
    try
    {
        json root;
        root["version"] = "0.0.009";
        root["fences"] = json::array();

        for (const auto& fence : fences)
        {
            root["fences"].push_back({
                {"id", ToUtf8(fence.id)},
                {"title", ToUtf8(fence.title)},
                {"x", fence.x},
                {"y", fence.y},
                {"width", fence.width},
                {"height", fence.height},
                {"backingFolder", ToUtf8(fence.backingFolder)},
                {"contentType", ToUtf8(fence.contentType)},
                {"contentPluginId", ToUtf8(fence.contentPluginId)},
                {"appearanceProfileId", ToUtf8(fence.appearanceProfileId)},
                {"widgetLayoutId", ToUtf8(fence.widgetLayoutId)}
            });
        }

        return SaveTextAtomic(root.dump(2));
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SaveFences exception for metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool Persistence::SaveFence(const FenceModel& fence)
{
    std::vector<FenceModel> fences;
    if (!LoadFences(fences))
    {
        return false;
    }

    bool updated = false;
    for (auto& existing : fences)
    {
        if (existing.id == fence.id)
        {
            existing = fence;
            updated = true;
            break;
        }
    }

    if (!updated)
    {
        fences.push_back(fence);
    }

    return SaveFences(fences);
}

bool Persistence::SaveTextAtomic(const std::string& text)
{
    try
    {
        const fs::path target(m_metadataPath);
        const fs::path tmp = target.wstring() + L".tmp";

        std::ofstream stream(tmp, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            Win32Helpers::LogError(L"Failed opening temp metadata file: " + tmp.wstring());
            return false;
        }

        stream << text;
        stream.flush();
        stream.close();

        if (!Win32Helpers::ReplaceFileAtomically(tmp, target))
        {
            Win32Helpers::LogError(L"SaveTextAtomic replace failed target='" + target.wstring() + L"' temp='" + tmp.wstring() + L"'");
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SaveTextAtomic exception for metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}
