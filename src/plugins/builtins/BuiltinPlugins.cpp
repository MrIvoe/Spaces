#include "plugins/builtins/BuiltinPlugins.h"

#include "AppVersion.h"
#include "core/CommandDispatcher.h"
#include "core/Diagnostics.h"
#include "extensions/FenceExtensionRegistry.h"
#include "extensions/MenuContributionRegistry.h"
#include "extensions/PluginContracts.h"
#include "extensions/PluginSettingsRegistry.h"
#include "extensions/SettingsSchema.h"
#include "Win32Helpers.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <windows.h>

#include <shellapi.h>

namespace
{
namespace fs = std::filesystem;

bool IsHiddenPath(const fs::path& path)
{
#ifdef _WIN32
    const auto attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    return false;
#endif
}

std::wstring SanitizeExtension(const fs::path& path)
{
    std::wstring ext = path.extension().wstring();
    if (ext.empty())
    {
        return L"no_extension";
    }

    if (!ext.empty() && ext.front() == L'.')
    {
        ext.erase(ext.begin());
    }

    for (auto& ch : ext)
    {
        if (ch >= L'A' && ch <= L'Z')
        {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
        if (ch == L' ')
        {
            ch = L'_';
        }
    }

    return ext.empty() ? L"no_extension" : ext;
}

fs::path BuildUniquePath(const fs::path& target)
{
    if (!fs::exists(target))
    {
        return target;
    }

    const fs::path stem = target.stem();
    const fs::path ext = target.extension();
    const fs::path dir = target.parent_path();
    for (int i = 1; i < 1000; ++i)
    {
        fs::path candidate = dir / (stem.wstring() + L" (" + std::to_wstring(i) + L")" + ext.wstring());
        if (!fs::exists(candidate))
        {
            return candidate;
        }
    }

    return target;
}

int GetSystemIconIndex(const fs::path& path)
{
    SHFILEINFOW shellInfo{};
    if (SHGetFileInfoW(path.c_str(), 0, &shellInfo, sizeof(shellInfo), SHGFI_SYSICONINDEX | SHGFI_SMALLICON) == 0)
    {
        return -1;
    }

    return shellInfo.iIcon;
}

bool CopyTextToClipboard(const std::wstring& text)
{
    if (text.empty())
    {
        return false;
    }

    if (!OpenClipboard(nullptr))
    {
        return false;
    }

    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
    {
        CloseClipboard();
        return false;
    }

    void* locked = GlobalLock(memory);
    if (!locked)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    memcpy(locked, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory))
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool MovePathToFolder(const fs::path& source, const fs::path& folder)
{
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec)
    {
        return false;
    }

    const fs::path destination = BuildUniquePath(folder / source.filename());
    fs::rename(source, destination, ec);
    if (!ec)
    {
        return true;
    }

    ec.clear();
    fs::copy(source, destination, fs::copy_options::recursive, ec);
    if (ec)
    {
        return false;
    }

    ec.clear();
    if (fs::is_directory(source, ec))
    {
        fs::remove_all(source, ec);
    }
    else
    {
        fs::remove(source, ec);
    }

    return !ec;
}

uint64_t ComputeFolderFingerprint(const fs::path& root, bool recurseSubfolders)
{
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
    {
        return 0;
    }

    uint64_t fingerprint = 1469598103934665603ull;
    auto mix = [&](uint64_t value) {
        fingerprint ^= value;
        fingerprint *= 1099511628211ull;
    };

    if (recurseSubfolders)
    {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            mix(std::hash<std::wstring>{}(entry.path().wstring()));
            const auto writeTime = fs::last_write_time(entry.path(), ec);
            if (!ec)
            {
                mix(static_cast<uint64_t>(writeTime.time_since_epoch().count()));
            }
            ec.clear();
        }
    }
    else
    {
        for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            mix(std::hash<std::wstring>{}(entry.path().wstring()));
            const auto writeTime = fs::last_write_time(entry.path(), ec);
            if (!ec)
            {
                mix(static_cast<uint64_t>(writeTime.time_since_epoch().count()));
            }
            ec.clear();
        }
    }

    return fingerprint;
}
} // namespace

class CoreCommandsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.core_commands";
        manifest.displayName = L"Core Commands";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Registers stable core command routes and core file_collection provider.";
        manifest.capabilities = {L"commands", L"fence_content_provider"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"core.file_collection", L"file_collection", L"File Collection", true});
        }

        if (context.settingsRegistry)
        {
            // --- Behavior page ---
            PluginSettingsPage behavior;
            behavior.pluginId = L"builtin.core_commands";
            behavior.pageId   = L"core.behavior";
            behavior.title    = L"Behavior";
            behavior.order    = 10;

            SettingsFieldDescriptor autoFocus;
            autoFocus.key          = L"core.behavior.auto_focus_on_create";
            autoFocus.label        = L"Auto-focus new fence";
            autoFocus.description  = L"Show and focus the fence window immediately after creation.";
            autoFocus.type         = SettingsFieldType::Bool;
            autoFocus.defaultValue = L"true";
            autoFocus.order        = 10;
            behavior.fields.push_back(std::move(autoFocus));

            SettingsFieldDescriptor defaultTitle;
            defaultTitle.key          = L"core.behavior.default_title";
            defaultTitle.label        = L"Default fence title";
            defaultTitle.description  = L"Title template applied to newly created fences.";
            defaultTitle.type         = SettingsFieldType::String;
            defaultTitle.defaultValue = L"New Fence";
            defaultTitle.order        = 20;
            behavior.fields.push_back(std::move(defaultTitle));

            context.settingsRegistry->RegisterPage(std::move(behavior));

            // --- Content Provider page ---
            PluginSettingsPage provider;
            provider.pluginId = L"builtin.core_commands";
            provider.pageId   = L"core.provider";
            provider.title    = L"Content Provider";
            provider.order    = 20;

            SettingsFieldDescriptor strictMode;
            strictMode.key          = L"core.provider.strict_mode";
            strictMode.label        = L"Strict provider mode";
            strictMode.description  = L"Reject unrecognised provider ids instead of normalising to core.file_collection.";
            strictMode.type         = SettingsFieldType::Bool;
            strictMode.defaultValue = L"false";
            strictMode.order        = 10;
            provider.fields.push_back(std::move(strictMode));

            context.settingsRegistry->RegisterPage(std::move(provider));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class TrayPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.tray";
        manifest.displayName = L"Tray Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Contributes tray menu items routed through commands.";
        manifest.capabilities = {L"tray_contributions", L"commands"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (!context.menuRegistry)
        {
            return false;
        }

        if (context.settingsRegistry)
        {
            PluginSettingsPage trayPage;
            trayPage.pluginId = L"builtin.tray";
            trayPage.pageId   = L"tray.behavior";
            trayPage.title    = L"Tray Behavior";
            trayPage.order    = 10;

            SettingsFieldDescriptor closeToTray;
            closeToTray.key          = L"tray.behavior.close_to_tray";
            closeToTray.label        = L"Close to tray";
            closeToTray.description  = L"Closing the main window hides it instead of exiting.";
            closeToTray.type         = SettingsFieldType::Bool;
            closeToTray.defaultValue = L"true";
            closeToTray.order        = 10;
            trayPage.fields.push_back(std::move(closeToTray));

            SettingsFieldDescriptor startMinimized;
            startMinimized.key          = L"tray.behavior.start_minimized";
            startMinimized.label        = L"Start minimized";
            startMinimized.description  = L"Launch the app without showing any window; only the tray icon appears.";
            startMinimized.type         = SettingsFieldType::Bool;
            startMinimized.defaultValue = L"false";
            startMinimized.order        = 20;
            trayPage.fields.push_back(std::move(startMinimized));

            context.settingsRegistry->RegisterPage(std::move(trayPage));
        }

        context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"New Fence", L"fence.create", 10, false});
        context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"Exit", L"app.exit", 1000, true});
        return true;
    }

    void Shutdown() override
    {
    }
};

class SettingsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.settings";
        manifest.displayName = L"Settings Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Registers settings-page scaffolding.";
        manifest.capabilities = {L"settings_pages", L"menu_contributions"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (!context.settingsRegistry)
        {
            return false;
        }

        PluginSettingsPage generalPage;
        generalPage.pluginId = L"builtin.settings";
        generalPage.pageId = L"general";
        generalPage.title = L"General";
        generalPage.order = 10;
        generalPage.fields = {
            SettingsFieldDescriptor{
                L"settings.ui.nav_collapsed",
                L"Compact sidebar (icons only)",
                L"Collapse the settings navigation rail to icons only.",
                SettingsFieldType::Bool,
                L"false",
                {},
                10}
        };
        context.settingsRegistry->RegisterPage(std::move(generalPage));

        PluginSettingsPage pluginsPage;
        pluginsPage.pluginId = L"builtin.settings";
        pluginsPage.pageId = L"plugins";
        pluginsPage.title = L"Plugins";
        pluginsPage.order = 20;
        pluginsPage.fields = {
            SettingsFieldDescriptor{
                L"settings.plugins.hub_repo_url",
                L"Plugin hub repository URL",
                L"Git repository used to download plugin folders.",
                SettingsFieldType::String,
                L"https://github.com/MrIvoe/Simple-Fences-Plugins.git",
                {},
                10},
            SettingsFieldDescriptor{
                L"settings.plugins.hub_branch",
                L"Plugin hub branch",
                L"Branch to pull plugin updates from.",
                SettingsFieldType::String,
                L"main",
                {},
                20},
            SettingsFieldDescriptor{
                L"settings.plugins.hub_action",
                L"Plugin hub action",
                L"Select 'Sync now' to pull from the repo and copy into the local plugins folder.",
                SettingsFieldType::Enum,
                L"idle",
                {
                    SettingsEnumOption{L"idle", L"Idle"},
                    SettingsEnumOption{L"sync_now", L"Sync now"}
                },
                30},
            SettingsFieldDescriptor{
                L"settings.plugins.manager_filter_status",
                L"Plugin manager status filter",
                L"Filter plugin list by runtime status and compatibility.",
                SettingsFieldType::Enum,
                L"all",
                {
                    SettingsEnumOption{L"all", L"All"},
                    SettingsEnumOption{L"loaded", L"Loaded"},
                    SettingsEnumOption{L"failed", L"Failed"},
                    SettingsEnumOption{L"disabled", L"Disabled"},
                    SettingsEnumOption{L"incompatible", L"Incompatible"}
                },
                40},
            SettingsFieldDescriptor{
                L"settings.plugins.manager_filter_text",
                L"Plugin manager text filter",
                L"Filter by plugin id or display name (applies after leaving the field).",
                SettingsFieldType::String,
                L"",
                {},
                50},
            SettingsFieldDescriptor{
                L"settings.plugins.manager_target_plugin",
                L"Plugin manager target plugin id",
                L"Exact plugin id to enable/disable (example: community.visual_modes).",
                SettingsFieldType::String,
                L"",
                {},
                60},
            SettingsFieldDescriptor{
                L"settings.plugins.manager_action",
                L"Plugin manager action",
                L"Apply enable/disable override for target plugin; change takes effect on next plugin host load.",
                SettingsFieldType::Enum,
                L"idle",
                {
                    SettingsEnumOption{L"idle", L"Idle"},
                    SettingsEnumOption{L"disable_selected", L"Disable selected plugin"},
                    SettingsEnumOption{L"enable_selected", L"Enable selected plugin"}
                },
                70}
        };
        context.settingsRegistry->RegisterPage(std::move(pluginsPage));

        context.settingsRegistry->RegisterPage(PluginSettingsPage{L"builtin.settings", L"diagnostics", L"Diagnostics", 30});

        if (context.menuRegistry)
        {
            context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"Settings", L"plugin.openSettings", 900, false});
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class AppearancePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"community.visual_modes";
        manifest.displayName = L"Appearance Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Theme controls plus visual mode commands for live fence presentation changes.";
        manifest.capabilities = {L"appearance", L"settings_pages", L"commands", L"menu_contributions"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        m_context = context;
        if (context.settingsRegistry)
        {
            context.settingsRegistry->RegisterPage(PluginSettingsPage{L"community.visual_modes", L"appearance.theme", L"Theme", 10,
            {
                SettingsFieldDescriptor{
                    L"appearance.theme.mode",
                    L"Theme mode",
                    L"Follow the Windows system preference, or force a fixed theme.",
                    SettingsFieldType::Enum,
                    L"system",
                    {
                        SettingsEnumOption{L"system", L"Follow system"},
                        SettingsEnumOption{L"light",  L"Always light"},
                        SettingsEnumOption{L"dark",   L"Always dark"},
                    },
                    10
                },
                SettingsFieldDescriptor{
                    L"theme.source",
                    L"Theme system",
                    L"Active theme catalog source. Win32ThemeSystem is the single supported catalog.",
                    SettingsFieldType::Enum,
                    L"win32_theme_system",
                    {
                        SettingsEnumOption{L"win32_theme_system", L"Win32 Theme System"},
                    },
                    20
                },
                SettingsFieldDescriptor{
                    L"theme.win32.theme_id",
                    L"Theme",
                    L"Active theme from the Win32ThemeSystem catalog.",
                    SettingsFieldType::Enum,
                    L"graphite-office",
                    {
                        SettingsEnumOption{L"amber-terminal",   L"Amber Terminal"},
                        SettingsEnumOption{L"arctic-glass",     L"Arctic Glass"},
                        SettingsEnumOption{L"aurora-light",     L"Aurora Light"},
                        SettingsEnumOption{L"brass-steampunk",  L"Brass Steampunk"},
                        SettingsEnumOption{L"copper-foundry",   L"Copper Foundry"},
                        SettingsEnumOption{L"emerald-ledger",   L"Emerald Ledger"},
                        SettingsEnumOption{L"forest-organic",   L"Forest Organic"},
                        SettingsEnumOption{L"graphite-office",  L"Graphite Office"},
                        SettingsEnumOption{L"harbor-blue",      L"Harbor Blue"},
                        SettingsEnumOption{L"ivory-bureau",     L"Ivory Bureau"},
                        SettingsEnumOption{L"mono-minimal",     L"Mono Minimal"},
                        SettingsEnumOption{L"neon-cyberpunk",   L"Neon Cyberpunk"},
                        SettingsEnumOption{L"nocturne-dark",    L"Nocturne Dark"},
                        SettingsEnumOption{L"nova-futuristic",  L"Nova Futuristic"},
                        SettingsEnumOption{L"olive-terminal",   L"Olive Terminal"},
                        SettingsEnumOption{L"pop-colorburst",   L"Pop Colorburst"},
                        SettingsEnumOption{L"rose-paper",       L"Rose Paper"},
                        SettingsEnumOption{L"storm-steel",      L"Storm Steel"},
                        SettingsEnumOption{L"sunset-retro",     L"Sunset Retro"},
                        SettingsEnumOption{L"tape-lo-fi",       L"Tape Lo-Fi"},
                    },
                    25
                },
                SettingsFieldDescriptor{
                    L"appearance.theme.text_scale_percent",
                    L"Text scale (%)",
                    L"UI text scaling for the host settings shell.",
                    SettingsFieldType::Int,
                    L"115",
                    {},
                    30
                },
            }});

                PluginSettingsPage visualModes;
                visualModes.pluginId = L"community.visual_modes";
                visualModes.pageId = L"appearance.visual_modes";
                visualModes.title = L"Visual Modes";
                visualModes.order = 20;
                visualModes.fields.push_back(SettingsFieldDescriptor{
                    L"appearance.visual_modes.apply_to_all_default",
                    L"Apply visual mode to all fences",
                    L"When enabled, visual mode commands update every fence instead of just the targeted one.",
                    SettingsFieldType::Bool,
                    L"false",
                    {},
                    10});
            }

            {
            }

            if (context.commandDispatcher)
            {
                context.commandDispatcher->RegisterCommand(L"appearance.mode.focus", [this](const CommandContext& command) {
                    ApplyVisualMode(command, L"focus");
                });
                context.commandDispatcher->RegisterCommand(L"appearance.mode.gallery", [this](const CommandContext& command) {
                    ApplyVisualMode(command, L"gallery");
                });
                context.commandDispatcher->RegisterCommand(L"appearance.mode.quiet", [this](const CommandContext& command) {
                    ApplyVisualMode(command, L"quiet");
                });
        }

        return true;
    }

    void Shutdown() override
    {
    }

    private:
        void ApplyVisualMode(const CommandContext& command, const std::wstring& mode) const
        {
            if (!m_context.appCommands)
            {
                return;
            }

            std::wstring fenceId = command.fence.id;
            if (fenceId.empty())
            {
                fenceId = m_context.appCommands->GetCurrentCommandContext().fence.id;
            }
            if (fenceId.empty())
            {
                return;
            }

            FencePresentationSettings settings;
            settings.applyToAll = m_context.settingsRegistry &&
                m_context.settingsRegistry->GetValue(L"appearance.visual_modes.apply_to_all_default", L"false") == L"true";

            if (mode == L"focus")
            {
                settings.textOnlyMode = false;
                settings.rollupWhenNotHovered = false;
                settings.transparentWhenNotHovered = false;
                settings.labelsOnHover = true;
                settings.iconSpacingPreset = L"spacious";
            }
            else if (mode == L"gallery")
            {
                settings.textOnlyMode = false;
                settings.rollupWhenNotHovered = false;
                settings.transparentWhenNotHovered = false;
                settings.labelsOnHover = false;
                settings.iconSpacingPreset = L"comfortable";
            }
            else
            {
                settings.textOnlyMode = true;
                settings.rollupWhenNotHovered = false;
                settings.transparentWhenNotHovered = false;
                settings.labelsOnHover = true;
                settings.iconSpacingPreset = L"compact";
            }

            m_context.appCommands->UpdateFencePresentation(fenceId, settings);
        }

        PluginContext m_context{};
};

class ExplorerFencePlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.explorer_portal";
        manifest.displayName = L"Explorer Fence Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Folder portal provider with source picking, health tracking, and live refresh polling.";
        manifest.capabilities = {L"fence_content_provider", L"settings_pages", L"commands", L"menu_contributions"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        m_context = context;
        if (context.fenceExtensionRegistry)
        {
            FenceContentProviderCallbacks callbacks;
            callbacks.enumerateItems = [this](const FenceMetadata& fence) {
                return EnumeratePortalItems(fence);
            };
            callbacks.handleDrop = [this](const FenceMetadata& fence, const std::vector<std::wstring>& paths) {
                return HandlePortalDrop(fence, paths);
            };
            callbacks.deleteItem = [this](const FenceMetadata& fence, const FenceItem& item) {
                return DeletePortalItem(fence, item);
            };
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"builtin.explorer_portal", L"folder_portal", L"Folder Portal"},
                callbacks);
        }

        if (context.settingsRegistry)
        {
            PluginSettingsPage portalPage;
            portalPage.pluginId = L"builtin.explorer_portal";
            portalPage.pageId   = L"explorer.portal";
            portalPage.title    = L"Portal Behavior";
            portalPage.order    = 10;

            portalPage.fields.push_back(SettingsFieldDescriptor{
                L"explorer.portal.recurse_subfolders",
                L"Include subfolders",
                L"Show items from subdirectories in the folder portal fence.",
                SettingsFieldType::Bool, L"false", {}, 10});
            portalPage.fields.push_back(SettingsFieldDescriptor{
                L"explorer.portal.show_hidden",
                L"Show hidden files",
                L"Include files and folders marked as Hidden.",
                SettingsFieldType::Bool, L"false", {}, 20});
            portalPage.fields.push_back(SettingsFieldDescriptor{
                L"explorer.portal.watch_interval_seconds",
                L"Watch interval (s)",
                L"Polling interval for portal reconnect and refresh detection. Lower values react faster but cost more IO.",
                SettingsFieldType::Int, L"2", {}, 30});

            context.settingsRegistry->RegisterPage(std::move(portalPage));
        }

        if (context.menuRegistry)
        {
            context.menuRegistry->Register(MenuContribution{MenuSurface::Tray, L"New Folder Portal", L"portal.create", 40, false});
            context.menuRegistry->Register(MenuContribution{MenuSurface::FenceContext, L"Change Portal Source...", L"portal.change_source", 200, true});
            context.menuRegistry->Register(MenuContribution{MenuSurface::FenceContext, L"Open Portal Source", L"portal.open_source", 210, false});
            context.menuRegistry->Register(MenuContribution{MenuSurface::FenceContext, L"Reconnect Portal", L"portal.reconnect", 220, false});
        }

        if (context.commandDispatcher)
        {
            context.commandDispatcher->RegisterCommand(L"portal.create", [this](const CommandContext&) {
                CreatePortalFence();
            });
            context.commandDispatcher->RegisterCommand(L"portal.change_source", [this](const CommandContext& command) {
                ChangePortalSource(command);
            });
            context.commandDispatcher->RegisterCommand(L"portal.open_source", [this](const CommandContext& command) {
                OpenPortalSource(command);
            });
            context.commandDispatcher->RegisterCommand(L"portal.reconnect", [this](const CommandContext& command) {
                ReconnectPortal(command);
            });
        }

        StartWatcher();

        return true;
    }

    void Shutdown() override
    {
        StopWatcher();
    }

private:
    struct PortalSnapshot
    {
        std::wstring sourcePath;
        uint64_t fingerprint = 0;
        bool reachable = false;
    };

    bool GetBoolSetting(const std::wstring& key, const std::wstring& fallback) const
    {
        return m_context.settingsRegistry && m_context.settingsRegistry->GetValue(key, fallback) == L"true";
    }

    int GetIntSetting(const std::wstring& key, const std::wstring& fallback) const
    {
        if (!m_context.settingsRegistry)
        {
            return _wtoi(fallback.c_str());
        }

        return _wtoi(m_context.settingsRegistry->GetValue(key, fallback).c_str());
    }

    void LogInfo(const std::wstring& message) const
    {
        if (m_context.diagnostics)
        {
            m_context.diagnostics->Info(L"[FolderPortal] " + message);
        }
    }

    void LogWarn(const std::wstring& message) const
    {
        if (m_context.diagnostics)
        {
            m_context.diagnostics->Warn(L"[FolderPortal] " + message);
        }
    }

    bool IsPortalFence(const FenceMetadata& fence) const
    {
        return fence.contentType == L"folder_portal" && fence.contentPluginId == L"builtin.explorer_portal";
    }

    std::vector<FenceItem> EnumeratePortalItems(const FenceMetadata& fence)
    {
        std::vector<FenceItem> items;
        if (!IsPortalFence(fence))
        {
            return items;
        }

        if (fence.contentSource.empty())
        {
            return items;
        }

        const fs::path root(fence.contentSource);
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
        {
            return items;
        }

        const bool recurse = GetBoolSetting(L"explorer.portal.recurse_subfolders", L"false");
        const bool showHidden = GetBoolSetting(L"explorer.portal.show_hidden", L"false");
        auto appendEntry = [&](const fs::directory_entry& entry) {
            const fs::path path = entry.path();
            if (!showHidden && IsHiddenPath(path))
            {
                return;
            }

            FenceItem item;
            item.name = path.filename().wstring();
            item.fullPath = path.wstring();
            item.isDirectory = entry.is_directory();
            item.iconIndex = GetSystemIconIndex(path);
            items.push_back(std::move(item));
        };

        if (recurse)
        {
            for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                appendEntry(entry);
            }
        }
        else
        {
            for (const auto& entry : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                appendEntry(entry);
            }
        }
        return items;
    }

    bool HandlePortalDrop(const FenceMetadata& fence, const std::vector<std::wstring>& paths)
    {
        if (!IsPortalFence(fence) || fence.contentSource.empty())
        {
            return false;
        }

        const fs::path targetFolder(fence.contentSource);
        std::error_code ec;
        if (!fs::exists(targetFolder, ec) || !fs::is_directory(targetFolder, ec))
        {
            if (m_context.appCommands)
            {
                m_context.appCommands->UpdateFenceContentState(fence.id, L"disconnected", L"Cannot drop into an unavailable source folder.");
            }
            return false;
        }

        bool movedAny = false;
        for (const auto& rawPath : paths)
        {
            if (rawPath.empty())
            {
                continue;
            }

            if (MovePathToFolder(fs::path(rawPath), targetFolder))
            {
                movedAny = true;
            }
        }

        if (movedAny && m_context.appCommands)
        {
            m_context.appCommands->UpdateFenceContentState(fence.id, L"ready", L"Portal connected.");
        }

        return movedAny;
    }

    bool DeletePortalItem(const FenceMetadata& fence, const FenceItem& item)
    {
        if (!IsPortalFence(fence) || item.fullPath.empty())
        {
            return false;
        }

        std::error_code ec;
        const fs::path path(item.fullPath);
        if (fs::is_directory(path, ec))
        {
            fs::remove_all(path, ec);
            return !ec;
        }

        ec.clear();
        return fs::remove(path, ec);
    }

    void CreatePortalFence()
    {
        if (!m_context.appCommands)
        {
            return;
        }

        std::wstring selectedPath;
        if (!Win32Helpers::PromptSelectFolder(nullptr, L"Choose Folder Portal Source", selectedPath))
        {
            return;
        }

        FenceCreateRequest request;
        request.title = fs::path(selectedPath).filename().wstring();
        if (request.title.empty())
        {
            request.title = L"Folder Portal";
        }
        request.contentType = L"folder_portal";
        request.contentPluginId = L"builtin.explorer_portal";
        request.contentSource = selectedPath;

        const std::wstring fenceId = m_context.appCommands->CreateFenceNearCursor(request);
        if (!fenceId.empty())
        {
            m_context.appCommands->UpdateFenceContentState(fenceId, L"connecting", L"Connecting to source folder...");
            LogInfo(L"Created folder portal for source: " + selectedPath);
        }
    }

    void ChangePortalSource(const CommandContext& command)
    {
        if (!m_context.appCommands || command.fence.id.empty())
        {
            return;
        }

        std::wstring selectedPath;
        if (!Win32Helpers::PromptSelectFolder(nullptr, L"Choose New Portal Source", selectedPath))
        {
            return;
        }

        m_context.appCommands->UpdateFenceContentSource(command.fence.id, selectedPath);
        m_context.appCommands->UpdateFenceContentState(command.fence.id, L"connecting", L"Connecting to source folder...");
    }

    void OpenPortalSource(const CommandContext& command) const
    {
        const std::wstring sourcePath = command.fence.contentSource;
        if (sourcePath.empty())
        {
            return;
        }

        ShellExecuteW(nullptr, L"open", sourcePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void ReconnectPortal(const CommandContext& command)
    {
        if (!m_context.appCommands || command.fence.id.empty())
        {
            return;
        }

        m_context.appCommands->UpdateFenceContentState(command.fence.id, L"connecting", L"Reconnect requested.");
    }

    void StartWatcher()
    {
        if (!m_context.appCommands)
        {
            return;
        }

        m_stopRequested = false;
        m_watchThread = std::thread([this]() { WatchLoop(); });
    }

    void StopWatcher()
    {
        {
            std::lock_guard<std::mutex> lock(m_watchMutex);
            m_stopRequested = true;
        }
        m_watchCv.notify_all();
        if (m_watchThread.joinable())
        {
            m_watchThread.join();
        }
    }

    void WatchLoop()
    {
        bool firstPass = true;
        for (;;)
        {
            {
                std::unique_lock<std::mutex> lock(m_watchMutex);
                if (!firstPass)
                {
                    const int intervalSeconds = max(1, GetIntSetting(L"explorer.portal.watch_interval_seconds", L"2"));
                    if (m_watchCv.wait_for(lock, std::chrono::seconds(intervalSeconds), [this]() { return m_stopRequested; }))
                    {
                        return;
                    }
                }
            }

            firstPass = false;

            if (!m_context.appCommands)
            {
                continue;
            }

            const bool recurse = GetBoolSetting(L"explorer.portal.recurse_subfolders", L"false");
            std::unordered_map<std::wstring, PortalSnapshot> nextSnapshots;
            for (const auto& fenceId : m_context.appCommands->GetAllFenceIds())
            {
                const FenceMetadata fence = m_context.appCommands->GetFenceMetadata(fenceId);
                if (!IsPortalFence(fence))
                {
                    continue;
                }

                PortalSnapshot snapshot;
                snapshot.sourcePath = fence.contentSource;

                std::error_code ec;
                const fs::path sourcePath(fence.contentSource);
                snapshot.reachable = !fence.contentSource.empty() && fs::exists(sourcePath, ec) && fs::is_directory(sourcePath, ec);
                snapshot.fingerprint = snapshot.reachable ? ComputeFolderFingerprint(sourcePath, recurse) : 0;
                nextSnapshots.emplace(fenceId, snapshot);

                PortalSnapshot previous;
                bool hadPrevious = false;
                {
                    std::lock_guard<std::mutex> lock(m_watchMutex);
                    const auto it = m_snapshots.find(fenceId);
                    if (it != m_snapshots.end())
                    {
                        previous = it->second;
                        hadPrevious = true;
                    }
                }

                if (!snapshot.reachable)
                {
                    m_context.appCommands->UpdateFenceContentState(fenceId, fence.contentSource.empty() ? L"needs_source" : L"disconnected", L"Source folder is unavailable.");
                    if (!hadPrevious || previous.reachable)
                    {
                        m_context.appCommands->RefreshFence(fenceId);
                    }
                    continue;
                }

                if (!hadPrevious || !previous.reachable || previous.sourcePath != snapshot.sourcePath || previous.fingerprint != snapshot.fingerprint)
                {
                    m_context.appCommands->UpdateFenceContentState(fenceId, L"ready", L"Portal connected.");
                    m_context.appCommands->RefreshFence(fenceId);
                }
            }

            std::lock_guard<std::mutex> lock(m_watchMutex);
            m_snapshots = std::move(nextSnapshots);
        }
    }

    PluginContext m_context{};
    std::thread m_watchThread;
    mutable std::mutex m_watchMutex;
    std::condition_variable m_watchCv;
    bool m_stopRequested = false;
    std::unordered_map<std::wstring, PortalSnapshot> m_snapshots;
};

class WidgetsPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.widgets";
        manifest.displayName = L"Widgets Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Placeholder for widget panel fences.";
        manifest.capabilities = {L"widgets", L"fence_content_provider"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        if (context.fenceExtensionRegistry)
        {
            context.fenceExtensionRegistry->RegisterContentProvider(
                FenceContentProviderDescriptor{L"builtin.widgets", L"widget_panel", L"Widget Panel"});
        }

        if (context.settingsRegistry)
        {
            // Widget Layout page
            PluginSettingsPage layoutPage;
            layoutPage.pluginId = L"builtin.widgets";
            layoutPage.pageId   = L"widgets.layout";
            layoutPage.title    = L"Widget Layout";
            layoutPage.order    = 10;

            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.compact_mode",
                L"Compact card mode",
                L"Display widget cards in a denser compact layout.",
                SettingsFieldType::Bool, L"false", {}, 10});
            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.snap_to_grid",
                L"Snap to grid",
                L"Snap widget positions to an invisible alignment grid.",
                SettingsFieldType::Bool, L"true", {}, 20});
            layoutPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.layout.grid_size",
                L"Grid size (px)",
                L"Pixel size of the snap grid when snap-to-grid is active.",
                SettingsFieldType::Int, L"16", {}, 30});

            context.settingsRegistry->RegisterPage(std::move(layoutPage));

            // Refresh Policy page
            PluginSettingsPage refreshPage;
            refreshPage.pluginId = L"builtin.widgets";
            refreshPage.pageId   = L"widgets.refresh";
            refreshPage.title    = L"Refresh Policy";
            refreshPage.order    = 20;

            refreshPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.refresh.interval_seconds",
                L"Refresh interval (s)",
                L"How often widget content is refreshed. Set 0 to disable automatic refresh.",
                SettingsFieldType::Int, L"60", {}, 10});
            refreshPage.fields.push_back(SettingsFieldDescriptor{
                L"widgets.refresh.pause_on_battery",
                L"Pause on battery",
                L"Stop automatic refreshes when the device is running on battery power.",
                SettingsFieldType::Bool, L"true", {}, 20});

            context.settingsRegistry->RegisterPage(std::move(refreshPage));
        }

        return true;
    }

    void Shutdown() override
    {
    }
};

class DesktopContextPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.desktop_context";
        manifest.displayName = L"Desktop Context Plugin";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Context-aware fence and item actions that receive routed payload metadata.";
        manifest.capabilities = {L"desktop_context", L"settings_pages", L"commands", L"menu_contributions"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        m_context = context;
        if (context.settingsRegistry)
        {
            PluginSettingsPage ctxPage;
            ctxPage.pluginId = L"builtin.desktop_context";
            ctxPage.pageId   = L"desktop.context";
            ctxPage.title    = L"Context Actions";
            ctxPage.order    = 10;

            ctxPage.fields.push_back(SettingsFieldDescriptor{
                L"desktop.context.show_quick_actions",
                L"Show fence quick actions",
                L"Display context-menu quick actions when right-clicking a fence.",
                SettingsFieldType::Bool, L"true", {}, 10});
            ctxPage.fields.push_back(SettingsFieldDescriptor{
                L"desktop.context.safety_confirmations",
                L"Confirm destructive actions",
                L"Ask for confirmation before deleting or clearing a fence.",
                SettingsFieldType::Bool, L"true", {}, 20});

            context.settingsRegistry->RegisterPage(std::move(ctxPage));
        }

        if (context.menuRegistry)
        {
            context.menuRegistry->Register(MenuContribution{MenuSurface::ItemContext, L"Copy Item Path", L"context.copy_item_path", 100, true});
            context.menuRegistry->Register(MenuContribution{MenuSurface::ItemContext, L"Open Item Parent Folder", L"context.open_item_parent", 110, false});
            context.menuRegistry->Register(MenuContribution{MenuSurface::FenceContext, L"Copy Fence Source", L"context.copy_fence_source", 120, true});
        }

        if (context.commandDispatcher)
        {
            context.commandDispatcher->RegisterCommand(L"context.copy_item_path", [this](const CommandContext& command) {
                CopyItemPath(command);
            });
            context.commandDispatcher->RegisterCommand(L"context.open_item_parent", [this](const CommandContext& command) {
                OpenItemParent(command);
            });
            context.commandDispatcher->RegisterCommand(L"context.copy_fence_source", [this](const CommandContext& command) {
                CopyFenceSource(command);
            });
        }

        return true;
    }

    void Shutdown() override
    {
    }

private:
    void LogInfo(const std::wstring& message) const
    {
        if (m_context.diagnostics)
        {
            m_context.diagnostics->Info(L"[ContextActions] " + message);
        }
    }

    void CopyItemPath(const CommandContext& command) const
    {
        if (!command.item.has_value())
        {
            return;
        }

        if (CopyTextToClipboard(command.item->fullPath))
        {
            LogInfo(L"Copied item path for '" + command.item->name + L"'.");
        }
    }

    void OpenItemParent(const CommandContext& command) const
    {
        if (!command.item.has_value())
        {
            return;
        }

        const fs::path parent = fs::path(command.item->fullPath).parent_path();
        if (!parent.empty())
        {
            ShellExecuteW(nullptr, L"open", parent.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void CopyFenceSource(const CommandContext& command) const
    {
        std::wstring text = command.fence.contentSource.empty()
            ? command.fence.backingFolderPath
            : command.fence.contentSource;
        if (!text.empty() && CopyTextToClipboard(text))
        {
            LogInfo(L"Copied fence source for '" + command.fence.title + L"'.");
        }
    }

    PluginContext m_context{};
};

class FenceOrganizerPlugin final : public IPlugin
{
public:
    PluginManifest GetManifest() const override
    {
        PluginManifest manifest;
        manifest.id = L"builtin.fence_organizer";
        manifest.displayName = L"Fence Organizer";
        manifest.version = SimpleFencesVersion::kVersion;
        manifest.description = L"Adds organize-by-type and cleanup actions for fence backing folders.";
        manifest.capabilities = {L"commands", L"menu_contributions", L"settings_pages"};
        return manifest;
    }

    bool Initialize(const PluginContext& context) override
    {
        m_context = context;
        if (!context.commandDispatcher || !context.settingsRegistry)
        {
            return false;
        }

        RegisterSettings(*context.settingsRegistry);
        RegisterMenu(context.menuRegistry);
        RegisterCommands(*context.commandDispatcher);
        return true;
    }

    void Shutdown() override {}

private:
    void RegisterSettings(PluginSettingsRegistry& settings)
    {
        PluginSettingsPage page;
        page.pluginId = L"builtin.fence_organizer";
        page.pageId = L"organizer.actions";
        page.title = L"Fence Organizer";
        page.order = 40;

        page.fields.push_back(SettingsFieldDescriptor{
            L"organizer.actions.folder_prefix",
            L"Organize folder prefix",
            L"Prefix used when creating extension buckets (e.g. _type_images).",
            SettingsFieldType::String,
            L"_type_",
            {},
            10});

        page.fields.push_back(SettingsFieldDescriptor{
            L"organizer.actions.include_hidden",
            L"Include hidden files",
            L"Include hidden files during organization operations.",
            SettingsFieldType::Bool,
            L"false",
            {},
            20});

        page.fields.push_back(SettingsFieldDescriptor{
            L"organizer.actions.skip_shortcuts",
            L"Skip shortcuts (.lnk)",
            L"Do not move shortcut files during organization operations.",
            SettingsFieldType::Bool,
            L"true",
            {},
            30});

        page.fields.push_back(SettingsFieldDescriptor{
            L"organizer.actions.only_managed_prefix",
            L"Flatten only managed folders",
            L"Only flatten folders with the configured prefix when enabled.",
            SettingsFieldType::Bool,
            L"true",
            {},
            40});

        settings.RegisterPage(std::move(page));
    }

    void RegisterMenu(MenuContributionRegistry* menu)
    {
        if (!menu)
        {
            return;
        }

        menu->Register(MenuContribution{MenuSurface::FenceContext, L"Organize by File Type", L"organizer.by_type", 500, true});
        menu->Register(MenuContribution{MenuSurface::FenceContext, L"Flatten Organized Folders", L"organizer.flatten", 510, false});
        menu->Register(MenuContribution{MenuSurface::FenceContext, L"Remove Empty Subfolders", L"organizer.cleanup_empty", 520, false});
    }

    void RegisterCommands(CommandDispatcher& dispatcher)
    {
        dispatcher.RegisterCommand(L"organizer.by_type", [this]() { OrganizeByType(); });
        dispatcher.RegisterCommand(L"organizer.flatten", [this]() { FlattenFolders(); });
        dispatcher.RegisterCommand(L"organizer.cleanup_empty", [this]() { RemoveEmptyFolders(); });
    }

    bool GetBoolSetting(const std::wstring& key, const std::wstring& fallback) const
    {
        if (!m_context.settingsRegistry)
        {
            return fallback == L"true";
        }

        const std::wstring value = m_context.settingsRegistry->GetValue(key, fallback);
        return value == L"true";
    }

    std::wstring GetStringSetting(const std::wstring& key, const std::wstring& fallback) const
    {
        if (!m_context.settingsRegistry)
        {
            return fallback;
        }

        return m_context.settingsRegistry->GetValue(key, fallback);
    }

    FenceMetadata CurrentFence() const
    {
        if (!m_context.appCommands)
        {
            return {};
        }

        return m_context.appCommands->GetActiveFenceMetadata();
    }

    void LogInfo(const std::wstring& message) const
    {
        if (m_context.diagnostics)
        {
            m_context.diagnostics->Info(L"[FenceOrganizer] " + message);
        }
    }

    void LogWarn(const std::wstring& message) const
    {
        if (m_context.diagnostics)
        {
            m_context.diagnostics->Warn(L"[FenceOrganizer] " + message);
        }
    }

    void OrganizeByType()
    {
        const auto fence = CurrentFence();
        if (fence.id.empty() || fence.backingFolderPath.empty())
        {
            LogWarn(L"No active fence under cursor; organize action skipped.");
            return;
        }

        const bool includeHidden = GetBoolSetting(L"organizer.actions.include_hidden", L"false");
        const bool skipShortcuts = GetBoolSetting(L"organizer.actions.skip_shortcuts", L"true");
        std::wstring prefix = GetStringSetting(L"organizer.actions.folder_prefix", L"_type_");
        if (prefix.empty())
        {
            prefix = L"_type_";
        }

        size_t movedCount = 0;
        size_t skippedCount = 0;

        try
        {
            const fs::path root(fence.backingFolderPath);
            if (!fs::exists(root) || !fs::is_directory(root))
            {
                LogWarn(L"Backing folder not available: " + fence.backingFolderPath);
                return;
            }

            for (const auto& entry : fs::directory_iterator(root))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const fs::path file = entry.path();
                if (!includeHidden && IsHiddenPath(file))
                {
                    ++skippedCount;
                    continue;
                }

                if (skipShortcuts && file.extension() == L".lnk")
                {
                    ++skippedCount;
                    continue;
                }

                const std::wstring extensionKey = SanitizeExtension(file);
                const fs::path bucket = root / (prefix + extensionKey);
                fs::create_directories(bucket);

                const fs::path destination = BuildUniquePath(bucket / file.filename());
                fs::rename(file, destination);
                ++movedCount;
            }

            if (m_context.appCommands)
            {
                m_context.appCommands->RefreshFence(fence.id);
            }

            LogInfo(L"Organized fence '" + fence.title + L"': moved " + std::to_wstring(movedCount) +
                    L" files, skipped " + std::to_wstring(skippedCount) + L" files.");
        }
        catch (const std::exception&)
        {
            LogWarn(L"Organize by type failed due to a filesystem exception.");
        }
    }

    void FlattenFolders()
    {
        const auto fence = CurrentFence();
        if (fence.id.empty() || fence.backingFolderPath.empty())
        {
            LogWarn(L"No active fence under cursor; flatten action skipped.");
            return;
        }

        const bool includeHidden = GetBoolSetting(L"organizer.actions.include_hidden", L"false");
        const bool skipShortcuts = GetBoolSetting(L"organizer.actions.skip_shortcuts", L"true");
        const bool onlyManaged = GetBoolSetting(L"organizer.actions.only_managed_prefix", L"true");
        const std::wstring prefix = GetStringSetting(L"organizer.actions.folder_prefix", L"_type_");

        size_t movedCount = 0;

        try
        {
            const fs::path root(fence.backingFolderPath);
            if (!fs::exists(root) || !fs::is_directory(root))
            {
                LogWarn(L"Backing folder not available: " + fence.backingFolderPath);
                return;
            }

            for (const auto& dirEntry : fs::directory_iterator(root))
            {
                if (!dirEntry.is_directory())
                {
                    continue;
                }

                const fs::path folder = dirEntry.path();
                const std::wstring folderName = folder.filename().wstring();
                if (onlyManaged && !prefix.empty() && folderName.rfind(prefix, 0) != 0)
                {
                    continue;
                }

                for (const auto& fileEntry : fs::directory_iterator(folder))
                {
                    if (!fileEntry.is_regular_file())
                    {
                        continue;
                    }

                    const fs::path file = fileEntry.path();
                    if (!includeHidden && IsHiddenPath(file))
                    {
                        continue;
                    }

                    if (skipShortcuts && file.extension() == L".lnk")
                    {
                        continue;
                    }

                    const fs::path destination = BuildUniquePath(root / file.filename());
                    fs::rename(file, destination);
                    ++movedCount;
                }
            }

            if (m_context.appCommands)
            {
                m_context.appCommands->RefreshFence(fence.id);
            }

            LogInfo(L"Flattened organized folders in '" + fence.title + L"': moved " + std::to_wstring(movedCount) + L" files.");
        }
        catch (const std::exception&)
        {
            LogWarn(L"Flatten folders failed due to a filesystem exception.");
        }
    }

    void RemoveEmptyFolders()
    {
        const auto fence = CurrentFence();
        if (fence.id.empty() || fence.backingFolderPath.empty())
        {
            LogWarn(L"No active fence under cursor; cleanup action skipped.");
            return;
        }

        size_t removedCount = 0;
        try
        {
            const fs::path root(fence.backingFolderPath);
            if (!fs::exists(root) || !fs::is_directory(root))
            {
                LogWarn(L"Backing folder not available: " + fence.backingFolderPath);
                return;
            }

            for (const auto& entry : fs::directory_iterator(root))
            {
                if (entry.is_directory() && fs::is_empty(entry.path()))
                {
                    fs::remove(entry.path());
                    ++removedCount;
                }
            }

            if (m_context.appCommands)
            {
                m_context.appCommands->RefreshFence(fence.id);
            }

            LogInfo(L"Removed " + std::to_wstring(removedCount) + L" empty folders from '" + fence.title + L"'.");
        }
        catch (const std::exception&)
        {
            LogWarn(L"Remove empty folders failed due to a filesystem exception.");
        }
    }

    PluginContext m_context{};
};

std::vector<std::unique_ptr<IPlugin>> CreateBuiltinPlugins()
{
    std::vector<std::unique_ptr<IPlugin>> plugins;
    plugins.push_back(std::make_unique<CoreCommandsPlugin>());
    plugins.push_back(std::make_unique<TrayPlugin>());
    plugins.push_back(std::make_unique<SettingsPlugin>());
    plugins.push_back(std::make_unique<AppearancePlugin>());
    plugins.push_back(std::make_unique<ExplorerFencePlugin>());
    plugins.push_back(std::make_unique<WidgetsPlugin>());
    plugins.push_back(std::make_unique<DesktopContextPlugin>());
    plugins.push_back(std::make_unique<FenceOrganizerPlugin>());
    return plugins;
}

