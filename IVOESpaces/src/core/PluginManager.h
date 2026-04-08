#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <vector>

struct PluginManifest;
struct IHostApi;

/// Represents a loaded plugin
struct LoadedPlugin {
    HMODULE moduleHandle;
    const PluginManifest* manifest;
    std::wstring pluginId;
    std::wstring filePath;
};

/// Manages plugin loading, initialization, and lifecycle
class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    /// Initialize plugin system
    bool Initialize(const std::wstring& pluginFolder, IHostApi* hostApi);

    /// Scan folder and load plugins
    bool LoadPlugins();

    /// Get loaded plugin by ID
    LoadedPlugin* GetPlugin(const std::wstring& pluginId);

    /// Check if plugin is loaded and enabled
    bool IsPluginEnabled(const std::wstring& pluginId);

    /// Get all loaded plugins
    const std::vector<std::unique_ptr<LoadedPlugin>>& GetLoadedPlugins() const;

    /// Shutdown all plugins
    void ShutdownAll();

private:
    bool LoadPlugin(const std::wstring& filePath);
    void UnloadPlugin(LoadedPlugin* plugin);

    std::wstring m_pluginFolder;
    IHostApi* m_hostApi;
    std::vector<std::unique_ptr<LoadedPlugin>> m_loadedPlugins;
    std::unordered_map<std::wstring, LoadedPlugin*> m_pluginMap;
};
