#pragma once

#include <cstdint>
#include <cstring>

/// Semantic version: major.minor.patch
struct PluginVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;

    uint32_t ToUint32() const {
        return ((major & 0xF) << 20) | ((minor & 0xFF) << 10) | (patch & 0x3FF);
    }

    static PluginVersion FromUint32(uint32_t v) {
        return {
            (uint16_t)((v >> 20) & 0xF),
            (uint16_t)((v >> 10) & 0xFF),
            (uint16_t)(v & 0x3FF)
        };
    }
};

/// Plugin capability flags
enum class PluginCapability : uint32_t {
    None                    = 0,
    AppMenu                 = 1 << 0,       // Can register commands in app menu
    SettingsPage            = 1 << 1,       // Can provide global settings page
    FenceSettingsPage       = 1 << 2,       // Can provide per-fence settings
    FenceViewProvider       = 1 << 3,       // Can provide custom fence content view
    RenderHook              = 1 << 4,       // Can hook into fence render pipeline
    CommandHandler          = 1 << 5,       // Can handle commands
    DesktopContextMenu      = 1 << 6,       // Can extend desktop context menu
    DragDropHandler         = 1 << 7        // Can handle drag/drop events
};

/// Plugin manifest - describes plugin identity and capabilities
struct PluginManifest {
    static constexpr uint32_t API_VERSION = 0x00000001;

    uint32_t apiVersion;                    // Must be API_VERSION
    const wchar_t* pluginId;                // Unique ID, e.g. "plugin.settings.menu"
    const wchar_t* displayName;             // User-visible name
    const wchar_t* author;                  // Plugin author
    PluginVersion version;                  // Semantic version
    const wchar_t* description;             // Short description
    uint32_t capabilities;                  // Bitmask of PluginCapability
};

/// Plugin lifecycle exports (must be in every plugin DLL)

extern "C" __declspec(dllexport) bool IVOE_Plugin_Initialize(void* hostApi);
extern "C" __declspec(dllexport) void IVOE_Plugin_Shutdown();
extern "C" __declspec(dllexport) const PluginManifest* IVOE_Plugin_GetManifest();
