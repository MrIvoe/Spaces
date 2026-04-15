#include "extensions/PluginHost.h"

#include "AppVersion.h"
#include "core/Diagnostics.h"
#include "core/CommandDispatcher.h"
#include "core/PluginAppearanceConflictGuard.h"
#include "extensions/SpaceExtensionRegistry.h"
#include "extensions/PluginSettingsRegistry.h"
#include "plugins/builtins/BuiltinPlugins.h"
#include "Win32Helpers.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <set>

#include <windows.h>

namespace
{
    using CreatePluginFn = IPlugin* (__cdecl*)();
    using DestroyPluginFn = void (__cdecl*)(IPlugin*);

    std::wstring Utf8ToWString(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0)
        {
            return L"(message conversion failed)";
        }

        std::wstring wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }

    bool EvaluateManifestCompatibility(const PluginManifest& manifest, std::wstring& reason)
    {
        if (manifest.id.empty())
        {
            reason = L"Manifest id is empty.";
            return false;
        }

        if (manifest.displayName.empty())
        {
            reason = L"Manifest displayName is empty.";
            return false;
        }

        if (manifest.version.empty())
        {
            reason = L"Manifest version is empty.";
            return false;
        }

        if (manifest.minHostApiVersion > manifest.maxHostApiVersion)
        {
            reason = L"Manifest minHostApiVersion is greater than maxHostApiVersion.";
            return false;
        }

        const int hostApi = SimpleSpacesVersion::kPluginApiVersion;
        if (hostApi < manifest.minHostApiVersion || hostApi > manifest.maxHostApiVersion)
        {
            reason = L"Plugin API range is incompatible with host API version " + std::to_wstring(hostApi) + L".";
            return false;
        }

        return true;
    }

    std::wstring JoinCommandIds(const std::vector<std::wstring>& commandIds)
    {
        if (commandIds.empty())
        {
            return L"(none)";
        }

        std::wstring result;
        for (size_t index = 0; index < commandIds.size(); ++index)
        {
            if (index > 0)
            {
                result += L", ";
            }
            result += commandIds[index];
        }

        return result;
    }

    // Safely load a DLL using SEH so that a corrupt or ABI-incompatible plugin
    // cannot raise a structured exception (e.g. STATUS_STACK_BUFFER_OVERRUN,
    // STATUS_INVALID_IMAGE_FORMAT) that would silently kill the host process.
    // Must be a standalone function — MSVC forbids C++ objects with destructors
    // inside __try blocks, so keep this free of any RAII types.
    HMODULE SafeLoadLibrary(const wchar_t* path) noexcept
    {
        HMODULE module = nullptr;
        __try
        {
            module = LoadLibraryW(path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            module = nullptr;
        }
        return module;
    }

    IPlugin* SafeCreatePlugin(CreatePluginFn createPlugin) noexcept
    {
        IPlugin* instance = nullptr;
        __try
        {
            instance = createPlugin ? createPlugin() : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            instance = nullptr;
        }
        return instance;
    }

    void SafeDestroyPlugin(DestroyPluginFn destroyPlugin, IPlugin* plugin) noexcept
    {
        if (!destroyPlugin || !plugin)
        {
            return;
        }

        __try
        {
            destroyPlugin(plugin);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    std::vector<std::filesystem::path> DiscoverExternalPluginDlls()
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
        {
            const std::filesystem::path exePath(modulePath);
            if (_wcsicmp(exePath.filename().c_str(), L"HostCoreTests.exe") == 0)
            {
                return {};
            }
        }

        std::vector<std::filesystem::path> dllPaths;
        const auto pluginRoot = Win32Helpers::GetAppDataRoot() / L"plugins";

        std::error_code ec;
        if (!std::filesystem::exists(pluginRoot, ec) || !std::filesystem::is_directory(pluginRoot, ec))
        {
            return dllPaths;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(pluginRoot, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!entry.is_regular_file(ec))
            {
                ec.clear();
                continue;
            }

            if (_wcsicmp(entry.path().extension().c_str(), L".dll") == 0)
            {
                dllPaths.push_back(entry.path());
            }
        }

        std::sort(dllPaths.begin(), dllPaths.end());
        return dllPaths;
    }

    bool ShouldLoadExternalPlugins(const PluginContext& context)
    {
        // Safety-first default: external plugins are disabled unless explicitly
        // enabled via environment variable for controlled troubleshooting.
        bool enabled = false;

        (void)context;

        wchar_t envValue[8]{};
        const DWORD envLen = GetEnvironmentVariableW(L"SIMPLESPACES_ENABLE_EXTERNAL_PLUGINS", envValue, static_cast<DWORD>(std::size(envValue)));
        if (envLen > 0)
        {
            enabled = (_wcsicmp(envValue, L"1") == 0 || _wcsicmp(envValue, L"true") == 0 || _wcsicmp(envValue, L"yes") == 0);
        }

        return enabled;
    }
}

struct PluginHost::LoadedPlugin
{
    using PluginDeleter = std::function<void(IPlugin*)>;

    std::unique_ptr<IPlugin, PluginDeleter> instance{nullptr, [](IPlugin* plugin) {
        delete plugin;
    }};
    HMODULE module = nullptr;
    std::wstring sourcePath;
    bool external = false;
};

PluginHost::PluginHost() = default;

PluginHost::~PluginHost()
{
    Shutdown();
}

bool PluginHost::LoadBuiltins(const PluginContext& context)
{
    m_commandDispatcher = context.commandDispatcher;
    m_registry.Clear();
    m_plugins.clear();
    m_registeredPluginCommands.clear();

    std::vector<std::unique_ptr<LoadedPlugin>> discoveredPlugins;

    try
    {
        auto builtinPlugins = CreateBuiltinPlugins();
        discoveredPlugins.reserve(builtinPlugins.size());
        for (auto& plugin : builtinPlugins)
        {
            auto loadedPlugin = std::make_unique<LoadedPlugin>();
            loadedPlugin->instance = std::unique_ptr<IPlugin, LoadedPlugin::PluginDeleter>(plugin.release(), [](IPlugin* instance) {
                delete instance;
            });
            loadedPlugin->sourcePath = L"builtin";
            discoveredPlugins.push_back(std::move(loadedPlugin));
        }
    }
    catch (const std::exception& ex)
    {
        if (context.diagnostics)
        {
            context.diagnostics->Error(L"Plugin creation failed: " + Utf8ToWString(ex.what()));
        }
        return false;
    }
    catch (...)
    {
        if (context.diagnostics)
        {
            context.diagnostics->Error(L"Plugin creation failed with unknown exception.");
        }
        return false;
    }

    if (!ShouldLoadExternalPlugins(context))
    {
        if (context.diagnostics)
        {
            context.diagnostics->Warn(L"External plugins are disabled by current policy. Set SIMPLESPACES_ENABLE_EXTERNAL_PLUGINS=1 to opt in.");
        }
    }
    else
    {
        for (const auto& dllPath : DiscoverExternalPluginDlls())
        {
            HMODULE module = SafeLoadLibrary(dllPath.c_str());
            if (!module)
            {
                if (context.diagnostics)
                {
                    context.diagnostics->Warn(
                        L"External plugin load failed (load error or SEH): path='" + dllPath.wstring() +
                        L"' win32err=" + std::to_wstring(GetLastError()));
                }
                continue;
            }

            auto createPlugin = reinterpret_cast<CreatePluginFn>(GetProcAddress(module, "CreatePlugin"));
            auto destroyPlugin = reinterpret_cast<DestroyPluginFn>(GetProcAddress(module, "DestroyPlugin"));
            if (!createPlugin || !destroyPlugin)
            {
                if (context.diagnostics)
                {
                    context.diagnostics->Warn(L"External plugin missing factory exports: path='" + dllPath.wstring() + L"'");
                }
                FreeLibrary(module);
                continue;
            }

            IPlugin* instance = SafeCreatePlugin(createPlugin);

            if (!instance)
            {
                if (context.diagnostics)
                {
                    context.diagnostics->Warn(L"External plugin factory failed (exception/SEH/null): path='" + dllPath.wstring() + L"'");
                }
                FreeLibrary(module);
                continue;
            }

            auto loadedPlugin = std::make_unique<LoadedPlugin>();
            loadedPlugin->module = module;
            loadedPlugin->external = true;
            loadedPlugin->sourcePath = dllPath.wstring();
            loadedPlugin->instance = std::unique_ptr<IPlugin, LoadedPlugin::PluginDeleter>(instance, [destroyPlugin](IPlugin* plugin) {
                SafeDestroyPlugin(destroyPlugin, plugin);
            });
            discoveredPlugins.push_back(std::move(loadedPlugin));
        }
    }

    bool allLoaded = true;
    std::set<std::wstring> seenPluginIds;
    for (auto& pluginSlot : discoveredPlugins)
    {
        IPlugin* plugin = pluginSlot->instance.get();
        PluginStatus status;
        try
        {
            status.manifest = plugin->GetManifest();
        }
        catch (const std::exception& ex)
        {
            status.enabled = false;
            status.loaded = false;
            status.lastError = L"GetManifest threw exception: " + Utf8ToWString(ex.what());
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(status.lastError);
            }
            continue;
        }
        catch (...)
        {
            status.enabled = false;
            status.loaded = false;
            status.lastError = L"GetManifest threw unknown exception.";
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(status.lastError);
            }
            continue;
        }

        std::wstring manifestCompatibilityReason;
        if (!EvaluateManifestCompatibility(status.manifest, manifestCompatibilityReason))
        {
            status.enabled = false;
            status.loaded = false;
            status.compatibilityStatus = L"incompatible";
            status.compatibilityReason = manifestCompatibilityReason;
            status.lastError = manifestCompatibilityReason;
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(L"Plugin manifest rejected: id='" + status.manifest.id + L"' reason='" + manifestCompatibilityReason + L"'");
            }
            continue;
        }
        status.compatibilityStatus = L"compatible";
        status.compatibilityReason = L"Host API range satisfied.";

        if (seenPluginIds.find(status.manifest.id) != seenPluginIds.end())
        {
            status.enabled = false;
            status.loaded = false;
            status.compatibilityStatus = L"rejected";
            status.compatibilityReason = L"Duplicate plugin id detected.";
            status.lastError = L"Duplicate plugin id detected.";
            allLoaded = false;
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Error(L"Plugin rejected due to duplicate id: " + status.manifest.id);
            }
            continue;
        }
        seenPluginIds.insert(status.manifest.id);

        status.enabled = status.manifest.enabledByDefault;
        if (context.settingsRegistry)
        {
            const std::wstring overrideKey = L"settings.plugins.enable." + status.manifest.id;
            const std::wstring overrideValue = context.settingsRegistry->GetValue(overrideKey, L"");
            if (overrideValue == L"true")
            {
                status.enabled = true;
            }
            else if (overrideValue == L"false")
            {
                status.enabled = false;
            }
        }

        if (!status.enabled)
        {
            m_registry.Upsert(status);
            if (context.diagnostics)
            {
                context.diagnostics->Info(L"Plugin disabled by default: " + status.manifest.id);
            }
            continue;
        }

        bool loaded = false;
        std::vector<std::wstring> commandIdsBeforeInit;
        size_t providerCount = 0;
        size_t settingsPageCount = 0;
        if (context.commandDispatcher)
        {
            commandIdsBeforeInit = context.commandDispatcher->ListCommandIds();
        }

        try
        {
            loaded = plugin->Initialize(context);
            status.loaded = loaded;
            if (!loaded)
            {
                status.lastError = L"Initialize returned false.";
                allLoaded = false;
            }
        }
        catch (const std::exception& ex)
        {
            status.loaded = false;
            status.lastError = L"Initialize threw exception: " + Utf8ToWString(ex.what());
            allLoaded = false;
        }
        catch (...)
        {
            status.loaded = false;
            status.lastError = L"Initialize threw unknown exception.";
            allLoaded = false;
        }

        if (loaded && context.commandDispatcher)
        {
            const auto commandIdsAfterInit = context.commandDispatcher->ListCommandIds();

            std::vector<std::wstring> pluginCommandIds;
            pluginCommandIds.reserve(commandIdsAfterInit.size());
            for (const auto& commandId : commandIdsAfterInit)
            {
                if (std::find(commandIdsBeforeInit.begin(), commandIdsBeforeInit.end(), commandId) == commandIdsBeforeInit.end())
                {
                    pluginCommandIds.push_back(commandId);
                }
            }

            PluginAppearanceConflictGuard conflictGuard;
            if (conflictGuard.HasAppearanceConflict(status.manifest.id, pluginCommandIds))
            {
                for (const auto& commandId : pluginCommandIds)
                {
                    context.commandDispatcher->UnregisterCommand(commandId);
                }

                status.loaded = false;
                status.lastError = L"Appearance command path disabled due to selector ownership conflict.";
                allLoaded = false;

                if (context.diagnostics)
                {
                    context.diagnostics->Warn(
                        L"Plugin appearance command path disabled: id='" + status.manifest.id +
                        L"' owner='" + PluginAppearanceConflictGuard::GetCanonicalAppearanceSelectorId() + L"'");
                }
            }
            else
            {
                m_registeredPluginCommands[status.manifest.id] = pluginCommandIds;
            }

            if (loaded && context.spaceExtensionRegistry)
            {
                const auto providers = context.spaceExtensionRegistry->GetContentProviders();
                providerCount = static_cast<size_t>(std::count_if(providers.begin(), providers.end(), [&status](const SpaceContentProviderDescriptor& descriptor) {
                    return descriptor.providerId == status.manifest.id;
                }));
            }

            if (loaded && context.settingsRegistry)
            {
                const auto pages = context.settingsRegistry->GetAllPages();
                settingsPageCount = static_cast<size_t>(std::count_if(pages.begin(), pages.end(), [&status](const PluginSettingsPage& page) {
                    return page.pluginId == status.manifest.id;
                }));
            }
        }

        m_registry.Upsert(status);

        if (context.diagnostics)
        {
            std::wstring capabilities;
            for (size_t i = 0; i < status.manifest.capabilities.size(); ++i)
            {
                capabilities += status.manifest.capabilities[i];
                if (i + 1 < status.manifest.capabilities.size())
                {
                    capabilities += L", ";
                }
            }

            if (status.loaded)
            {
                context.diagnostics->Info(
                    L"Plugin loaded: id='" + status.manifest.id +
                    L"' source='" + (pluginSlot->external ? L"external" : L"builtin") +
                    L"' path='" + pluginSlot->sourcePath +
                    L"' compatibility='" + status.compatibilityStatus +
                    L"' capabilities='" + capabilities +
                    L"' commands='" + JoinCommandIds(m_registeredPluginCommands[status.manifest.id]) +
                    L"' settingsPages=" + std::to_wstring(settingsPageCount) +
                    L" providers=" + std::to_wstring(providerCount));
            }
            else
            {
                context.diagnostics->Error(
                    L"Plugin failed: id='" + status.manifest.id +
                    L"' source='" + (pluginSlot->external ? L"external" : L"builtin") +
                    L"' path='" + pluginSlot->sourcePath +
                    L"' compatibility='" + status.compatibilityStatus +
                    L"' reason='" + status.compatibilityReason +
                    L"' error='" + status.lastError + L"'");
            }
        }
    }

    m_plugins = std::move(discoveredPlugins);

    return allLoaded;
}

bool PluginHost::ReloadBuiltins(const PluginContext& context)
{
    Shutdown();
    return LoadBuiltins(context);
}

void PluginHost::Shutdown()
{
    if (m_commandDispatcher)
    {
        for (const auto& entry : m_registeredPluginCommands)
        {
            for (const auto& commandId : entry.second)
            {
                m_commandDispatcher->UnregisterCommand(commandId);
            }
        }
    }
    m_registeredPluginCommands.clear();

    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
    {
        if ((*it) && (*it)->instance)
        {
            try
            {
                (*it)->instance->Shutdown();
            }
            catch (...)
            {
                // Keep shutdown resilient even if a plugin misbehaves.
            }

            (*it)->instance.reset();
            if ((*it)->module)
            {
                FreeLibrary((*it)->module);
                (*it)->module = nullptr;
            }
        }
    }

    m_plugins.clear();
    m_commandDispatcher = nullptr;
}

const PluginRegistry& PluginHost::GetRegistry() const
{
    return m_registry;
}

