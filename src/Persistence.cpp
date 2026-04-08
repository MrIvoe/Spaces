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

bool Persistence::LoadSpaces(std::vector<SpaceModel>& spaces)
{
    try
    {
        spaces.clear();

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
            file.close();
            Win32Helpers::LogError(L"Metadata file is malformed json. Quarantining file and starting with empty set: " + m_metadataPath);
            QuarantineCorruptMetadata(L"Malformed JSON root");
            return true;
        }

        if (!root.contains("spaces") || !root["spaces"].is_array())
        {
            return true;
        }

        for (const auto& item : root["spaces"])
        {
            if (!item.is_object())
            {
                continue;
            }

            SpaceModel space;
            space.id = FromUtf8(item.value("id", std::string{}));
            if (space.id.empty())
            {
                continue;
            }

            space.title = FromUtf8(item.value("title", std::string{"Space"}));
            space.x = item.value("x", 100);
            space.y = item.value("y", 100);
            space.width = item.value("width", 320);
            space.height = item.value("height", 240);
            space.backingFolder = FromUtf8(item.value("backingFolder", std::string{}));
            space.contentType = FromUtf8(item.value("contentType", std::string{"file_collection"}));
            space.contentPluginId = FromUtf8(item.value("contentPluginId", std::string{"core.file_collection"}));
            space.contentSource = FromUtf8(item.value("contentSource", std::string{}));
            space.contentState = FromUtf8(item.value("contentState", std::string{"ready"}));
            space.contentStateDetail = FromUtf8(item.value("contentStateDetail", std::string{}));
            space.appearanceProfileId = FromUtf8(item.value("appearanceProfileId", std::string{}));
            space.widgetLayoutId = FromUtf8(item.value("widgetLayoutId", std::string{}));
            space.textOnlyMode = item.value("textOnlyMode", false);
            space.rollupWhenNotHovered = item.value("rollupWhenNotHovered", false);
            space.transparentWhenNotHovered = item.value("transparentWhenNotHovered", false);
            space.labelsOnHover = item.value("labelsOnHover", true);
            space.iconSpacingPreset = FromUtf8(item.value("iconSpacingPreset", std::string{"comfortable"}));
            space.inheritThemePolicy = item.value("inheritThemePolicy", true);

            if (space.contentType.empty())
            {
                space.contentType = L"file_collection";
            }

            if (space.contentPluginId.empty())
            {
                space.contentPluginId = L"core.file_collection";
            }

            if (space.contentState.empty())
            {
                space.contentState = L"ready";
            }
            spaces.push_back(std::move(space));
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"LoadSpaces exception. Quarantining metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        QuarantineCorruptMetadata(L"Exception during load");
        spaces.clear();
        return true;
    }
}

bool Persistence::SaveSpaces(const std::vector<SpaceModel>& spaces)
{
    try
    {
        json root;
        root["version"] = "0.0.009";
        root["spaces"] = json::array();

        for (const auto& space : spaces)
        {
            root["spaces"].push_back({
                {"id", ToUtf8(space.id)},
                {"title", ToUtf8(space.title)},
                {"x", space.x},
                {"y", space.y},
                {"width", space.width},
                {"height", space.height},
                {"backingFolder", ToUtf8(space.backingFolder)},
                {"contentType", ToUtf8(space.contentType)},
                {"contentPluginId", ToUtf8(space.contentPluginId)},
                {"contentSource", ToUtf8(space.contentSource)},
                {"contentState", ToUtf8(space.contentState)},
                {"contentStateDetail", ToUtf8(space.contentStateDetail)},
                {"appearanceProfileId", ToUtf8(space.appearanceProfileId)},
                {"widgetLayoutId", ToUtf8(space.widgetLayoutId)},
                {"textOnlyMode", space.textOnlyMode},
                {"rollupWhenNotHovered", space.rollupWhenNotHovered},
                {"transparentWhenNotHovered", space.transparentWhenNotHovered},
                {"labelsOnHover", space.labelsOnHover},
                {"iconSpacingPreset", ToUtf8(space.iconSpacingPreset)},
                {"inheritThemePolicy", space.inheritThemePolicy}
            });
        }

        return SaveTextAtomic(root.dump(2));
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SaveSpaces exception for metadata path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}

bool Persistence::SaveSpace(const SpaceModel& space)
{
    std::vector<SpaceModel> spaces;
    if (!LoadSpaces(spaces))
    {
        return false;
    }

    bool updated = false;
    for (auto& existing : spaces)
    {
        if (existing.id == space.id)
        {
            existing = space;
            updated = true;
            break;
        }
    }

    if (!updated)
    {
        spaces.push_back(space);
    }

    return SaveSpaces(spaces);
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

bool Persistence::QuarantineCorruptMetadata(const std::wstring& reason)
{
    try
    {
        if (!fs::exists(m_metadataPath))
        {
            return true;
        }

        SYSTEMTIME st{};
        GetLocalTime(&st);

        const fs::path source(m_metadataPath);
        const fs::path backup = source.wstring() +
            L".corrupt-" +
            std::to_wstring(st.wYear) +
            std::to_wstring(st.wMonth) +
            std::to_wstring(st.wDay) +
            L"-" +
            std::to_wstring(st.wHour) +
            std::to_wstring(st.wMinute) +
            std::to_wstring(st.wSecond);

        std::error_code ec;
        fs::rename(source, backup, ec);
        if (ec)
        {
            Win32Helpers::LogError(L"Failed to quarantine corrupt metadata file: " + source.wstring() + L" reason='" + NarrowToWide(ec.message()) + L"'");
            return false;
        }

        Win32Helpers::LogInfo(L"Quarantined corrupt metadata file: " + backup.wstring() + L" reason='" + reason + L"'");
        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"QuarantineCorruptMetadata exception for path: " + m_metadataPath + L" reason: " + NarrowToWide(ex.what()));
        return false;
    }
}
