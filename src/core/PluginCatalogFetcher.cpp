#include "core/PluginCatalogFetcher.h"
#include "AppVersion.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

namespace fs = std::filesystem;

namespace PluginCatalog
{
    namespace
    {
        std::wstring Utf8ToWide(const std::string& s)
        {
            return std::wstring(s.begin(), s.end());
        }

        int ParseApiVersionMajor(const std::string& text)
        {
            if (text.empty())
            {
                return 0;
            }

            const size_t dot = text.find('.');
            const std::string head = (dot == std::string::npos) ? text : text.substr(0, dot);
            try
            {
                return std::stoi(head);
            }
            catch (...)
            {
                return 0;
            }
        }
    }

    CatalogFetcher::~CatalogFetcher() = default;

    bool CatalogFetcher::FetchCatalog(const std::wstring& source)
    {
        m_lastError.clear();

        // Check if source is a URL or file path
        if (source.find(L"http://") == 0 || source.find(L"https://") == 0)
        {
            return FetchFromUrl(source);
        }
        else
        {
            return LoadFromFile(source);
        }
    }

    bool CatalogFetcher::FetchFromUrl(const std::wstring& url)
    {
        HINTERNET hInternet = InternetOpenW(
            L"Spaces/1.0",
            INTERNET_OPEN_TYPE_PRECONFIG,
            nullptr,
            nullptr,
            0);

        if (!hInternet)
        {
            m_lastError = L"Failed to open internet connection.";
            return false;
        }

        HINTERNET hUrl = InternetOpenUrlW(
            hInternet,
            url.c_str(),
            nullptr,
            0,
            INTERNET_FLAG_RELOAD,
            0);

        if (!hUrl)
        {
            m_lastError = L"Failed to open URL: " + url;
            InternetCloseHandle(hInternet);
            return false;
        }

        std::string contentBuffer;
        const DWORD readBufferSize = 4096;
        std::vector<char> readBuffer(readBufferSize);

        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, readBuffer.data(), readBufferSize, &bytesRead))
        {
            if (bytesRead == 0)
                break;
            contentBuffer.append(readBuffer.data(), bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (contentBuffer.empty())
        {
            m_lastError = L"Downloaded catalog is empty.";
            return false;
        }

        try
        {
            json j = json::parse(contentBuffer);
            return ParseCatalog(j);
        }
        catch (const std::exception& e)
        {
            m_lastError = L"Failed to parse catalog JSON: ";
            m_lastError += std::wstring(e.what(), e.what() + strlen(e.what()));
            return false;
        }
    }

    bool CatalogFetcher::LoadFromFile(const std::wstring& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            m_lastError = L"Cannot open catalog file: " + filePath;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string contentString = buffer.str();

        try
        {
            json j = json::parse(contentString);
            return ParseCatalog(j);
        }
        catch (const std::exception& e)
        {
            m_lastError = L"Failed to parse catalog JSON: ";
            m_lastError += std::wstring(e.what(), e.what() + strlen(e.what()));
            return false;
        }
    }

    bool CatalogFetcher::ParseCatalog(const json& j)
    {
        try
        {
            m_catalog.version = j.value("version", 1);
            m_catalog.catalogVersion = std::wstring(j.value("catalogVersion", "").begin(), j.value("catalogVersion", "").end());
            m_catalog.lastUpdated = std::wstring(j.value("lastUpdated", "").begin(), j.value("lastUpdated", "").end());
            m_catalog.appMinVersion = std::wstring(j.value("appMinVersion", "").begin(), j.value("appMinVersion", "").end());

            m_catalog.plugins.clear();

            if (j.contains("plugins") && j["plugins"].is_array())
            {
                for (const auto& pluginJson : j["plugins"])
                {
                    PluginEntry entry;
                    const std::string id = pluginJson.value("id", "");
                    const std::string displayName = pluginJson.value("displayName", pluginJson.value("name", ""));
                    const std::string description = pluginJson.value("description", "");
                    const std::string version = pluginJson.value("version", "");
                    const std::string channel = pluginJson.value("channel", "stable");
                    const std::string author = pluginJson.value("author", "");
                    const std::string downloadUrl = pluginJson.value("downloadUrl", "");
                    const std::string hash = pluginJson.value("hash", pluginJson.value("sha256", ""));
                    const std::string category = pluginJson.value("category", "uncategorized");

                    entry.id = Utf8ToWide(id);
                    entry.displayName = Utf8ToWide(displayName);
                    entry.description = Utf8ToWide(description);
                    entry.version = Utf8ToWide(version);
                    entry.channel = Utf8ToWide(channel);
                    entry.author = Utf8ToWide(author);
                    entry.downloadUrl = Utf8ToWide(downloadUrl);
                    entry.hash = Utf8ToWide(hash);
                    entry.category = Utf8ToWide(category);

                    // Compatibility model: prefer object, fallback to legacy flat keys.
                    if (pluginJson.contains("compatibility") && pluginJson["compatibility"].is_object())
                    {
                        const auto& compat = pluginJson["compatibility"];
                        if (compat.contains("hostVersion") && compat["hostVersion"].is_object())
                        {
                            const auto& hostVersion = compat["hostVersion"];
                            entry.minHostVersion = Utf8ToWide(hostVersion.value("min", ""));
                            entry.maxHostVersion = Utf8ToWide(hostVersion.value("max", ""));
                        }

                        if (compat.contains("hostApiVersion") && compat["hostApiVersion"].is_object())
                        {
                            const auto& hostApiVersion = compat["hostApiVersion"];
                            entry.minHostApiVersion = ParseApiVersionMajor(hostApiVersion.value("min", "0"));
                            entry.maxHostApiVersion = ParseApiVersionMajor(hostApiVersion.value("max", "0"));
                        }
                    }
                    else
                    {
                        entry.minHostVersion = Utf8ToWide(pluginJson.value("minHostVersion", ""));
                        entry.maxHostVersion = Utf8ToWide(pluginJson.value("maxHostVersion", ""));
                        entry.minHostApiVersion = ParseApiVersionMajor(pluginJson.value("minHostApiVersion", "0"));
                        entry.maxHostApiVersion = ParseApiVersionMajor(pluginJson.value("maxHostApiVersion", "0"));
                    }

                    entry.supportsSettingsPage = pluginJson.value("supportsSettingsPage", false);
                    entry.restartRequired = pluginJson.value("restartRequired", true);

                    entry.enabled = pluginJson.value("enabled", true);
                    entry.installed = pluginJson.value("installed", false);

                    if (pluginJson.contains("capabilities") && pluginJson["capabilities"].is_array())
                    {
                        for (const auto& capability : pluginJson["capabilities"])
                        {
                            entry.capabilities.push_back(Utf8ToWide(capability.get<std::string>()));
                        }
                    }

                    if (pluginJson.contains("tags") && pluginJson["tags"].is_array())
                    {
                        for (const auto& tag : pluginJson["tags"])
                        {
                            entry.tags.push_back(Utf8ToWide(tag.get<std::string>()));
                        }
                    }

                    m_catalog.plugins.push_back(entry);
                }
            }

            return true;
        }
        catch (const std::exception& e)
        {
            m_lastError = L"Error parsing catalog structure: ";
            m_lastError += std::wstring(e.what(), e.what() + strlen(e.what()));
            return false;
        }
    }

    const PluginEntry* CatalogFetcher::FindPlugin(const std::wstring& pluginId) const
    {
        auto it = std::find_if(m_catalog.plugins.begin(), m_catalog.plugins.end(),
            [&pluginId](const PluginEntry& entry) { return entry.id == pluginId; });

        if (it != m_catalog.plugins.end())
        {
            return &(*it);
        }

        return nullptr;
    }

    bool CatalogFetcher::IsPluginCompatible(const PluginEntry& entry) const
    {
        // Check host API version compatibility
        if (entry.minHostApiVersion > 0 && entry.minHostApiVersion > SimpleSpacesVersion::kPluginApiVersion)
        {
            return false;
        }

        if (entry.maxHostApiVersion > 0 && entry.maxHostApiVersion < SimpleSpacesVersion::kPluginApiVersion)
        {
            return false;
        }

        // Version string comparison is simplified; in production, use semantic versioning
        // For now,just basic stability check
        return true;
    }

} // namespace PluginCatalog
