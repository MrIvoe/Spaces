#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PluginCatalog
{
    // Represents a single plugin entry in the catalog
    struct PluginEntry
    {
        std::wstring id;
        std::wstring displayName;
        std::wstring description;
        std::wstring version;
        std::wstring channel;
        std::wstring author;
        std::wstring downloadUrl;
        std::wstring hash;
        std::wstring minHostVersion;
        std::wstring maxHostVersion;
        int minHostApiVersion = 0;
        int maxHostApiVersion = 0;
        std::wstring category;
        std::vector<std::wstring> capabilities;
        bool supportsSettingsPage = false;
        bool restartRequired = true;
        std::vector<std::wstring> tags;
        bool enabled = true;
        bool installed = false;
    };

    // Represents the entire plugin catalog
    struct Catalog
    {
        int version = 1;
        std::wstring catalogVersion;
        std::wstring lastUpdated;
        std::wstring appMinVersion;
        std::vector<PluginEntry> plugins;
    };

    // High-level API for fetching and managing the plugin catalog
    class CatalogFetcher
    {
    public:
        CatalogFetcher() = default;
        ~CatalogFetcher();

        // Fetch catalog from URL or local file
        // Returns true on success
        bool FetchCatalog(const std::wstring& source);

        // Get the loaded catalog
        const Catalog& GetCatalog() const { return m_catalog; }

        // Find a plugin by ID
        const PluginEntry* FindPlugin(const std::wstring& pluginId) const;

        // Get last error message
        std::wstring GetLastError() const { return m_lastError; }

        // Check if a plugin is compatible with current host
        bool IsPluginCompatible(const PluginEntry& entry) const;

    private:
        Catalog m_catalog;
        std::wstring m_lastError;

        // Parse JSON catalog
        bool ParseCatalog(const json& j);

        // Fetch from HTTP URL
        bool FetchFromUrl(const std::wstring& url);

        // Load from local file
        bool LoadFromFile(const std::wstring& filePath);
    };

} // namespace PluginCatalog
