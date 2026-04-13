#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class SettingsStore;
struct UniversalThemeData;

/// Runtime-selectable theme resources: icon packs, component families, motion presets.
/// Resolves user appearance settings to actual resource definitions.
class ThemeResourceResolver
{
public:
    explicit ThemeResourceResolver(SettingsStore* settingsStore);
    ~ThemeResourceResolver() = default;

    /// Initialize resolver with loaded theme data.
    /// Must be called after theme is loaded.
    void Initialize(const UniversalThemeData* themeData);

    // Icon Pack Resolution ================================================

    /// Get default icon pack for current theme
    std::wstring GetDefaultIconPack() const;

    /// Get currently selected icon pack (from appearance.ui.icon_pack or default)
    std::wstring GetSelectedIconPack() const;

    /// Get all available icon packs
    std::vector<std::wstring> GetAvailableIconPacks() const;

    /// Get metadata for an icon pack (label, description, etc.)
    std::wstring GetIconPackLabel(const std::wstring& packId) const;
    std::wstring GetIconPackDescription(const std::wstring& packId) const;

    // Button Family Resolution ============================================

    /// Get default button family for current theme
    std::wstring GetDefaultButtonFamily() const;

    /// Get currently selected button family (from appearance.ui.button_family or default)
    std::wstring GetSelectedButtonFamily() const;

    /// Get all available button families
    std::vector<std::wstring> GetAvailableButtonFamilies() const;

    /// Get label for button family
    std::wstring GetButtonFamilyLabel(const std::wstring& familyId) const;

    // Control Family Resolution ===========================================

    /// Get default control family for current theme
    std::wstring GetDefaultControlFamily() const;

    /// Get currently selected control family (from appearance.ui.controls_family or default)
    std::wstring GetSelectedControlFamily() const;

    /// Get all available control families
    std::vector<std::wstring> GetAvailableControlFamilies() const;

    /// Get label for control family
    std::wstring GetControlFamilyLabel(const std::wstring& familyId) const;

    // Component Family Resolution (Overall Style Direction) ===============

    /// Get default component family (desktop-fluent, dashboard-modern, etc.)
    std::wstring GetDefaultComponentFamily() const;

    /// Get currently selected component family from appearance.ui.component_family
    std::wstring GetSelectedComponentFamily() const;

    /// Get all available component families
    std::vector<std::wstring> GetAvailableComponentFamilies() const;

    /// Get label and description for component family
    std::wstring GetComponentFamilyLabel(const std::wstring& familyId) const;
    std::wstring GetComponentFamilyDescription(const std::wstring& familyId) const;

    // Menu Style Resolution ===============================================

    /// Get default menu style
    std::wstring GetDefaultMenuStyle() const;

    /// Get currently selected menu style
    std::wstring GetSelectedMenuStyle() const;

    /// Get all available menu styles
    std::vector<std::wstring> GetAvailableMenuStyles() const;

    std::wstring GetMenuStyleLabel(const std::wstring& styleId) const;

    // Fence Shell Resolution ==============================================

    /// Get default fence shell style
    std::wstring GetDefaultFenceStyle() const;

    /// Get currently selected fence shell style
    std::wstring GetSelectedFenceStyle() const;

    /// Get all available fence styles
    std::vector<std::wstring> GetAvailableFenceStyles() const;

    std::wstring GetFenceStyleLabel(const std::wstring& styleId) const;

    // Motion Preset Resolution ============================================

    /// Get default motion preset
    std::wstring GetDefaultMotionPreset() const;

    /// Get currently selected motion preset
    std::wstring GetSelectedMotionPreset() const;

    /// Get all available motion presets
    std::vector<std::wstring> GetAvailableMotionPresets() const;

    std::wstring GetMotionPresetLabel(const std::wstring& presetId) const;

    /// Get motion duration in milliseconds from preset
    int GetMotionDurationMs(const std::wstring& presetId, int fallbackMs = 400) const;

    /// Get easing function string from preset
    std::wstring GetMotionEasing(const std::wstring& presetId) const;

    // Settings Persistence ===============================================

    /// Persist a resource selection to settings store
    void SetIconPack(const std::wstring& packId);
    void SetButtonFamily(const std::wstring& familyId);
    void SetControlFamily(const std::wstring& familyId);
    void SetComponentFamily(const std::wstring& familyId);
    void SetMenuStyle(const std::wstring& styleId);
    void SetFenceStyle(const std::wstring& styleId);
    void SetMotionPreset(const std::wstring& presetId);

    // Query Helpers ======================================================

    /// Check if a given pack/family/style is available
    bool IsIconPackAvailable(const std::wstring& packId) const;
    bool IsButtonFamilyAvailable(const std::wstring& familyId) const;
    bool IsControlFamilyAvailable(const std::wstring& familyId) const;
    bool IsComponentFamilyAvailable(const std::wstring& familyId) const;

    /// Get total count of resources
    size_t GetIconPackCount() const;
    size_t GetComponentFamilyCount() const;

private:
    SettingsStore* m_settingsStore = nullptr;
    const UniversalThemeData* m_themeData = nullptr;

    // Cached parsed resources (loaded on Initialize)
    std::vector<std::wstring> m_availableIconPacks;
    std::unordered_map<std::wstring, std::wstring> m_iconPackLabels;

    std::vector<std::wstring> m_availableButtonFamilies;
    std::unordered_map<std::wstring, std::wstring> m_buttonFamilyLabels;

    std::vector<std::wstring> m_availableControlFamilies;
    std::unordered_map<std::wstring, std::wstring> m_controlFamilyLabels;

    std::vector<std::wstring> m_availableComponentFamilies;
    std::unordered_map<std::wstring, std::wstring> m_componentFamilyLabels;
    std::unordered_map<std::wstring, std::wstring> m_componentFamilyDescriptions;

    std::vector<std::wstring> m_availableMenuStyles;
    std::unordered_map<std::wstring, std::wstring> m_menuStyleLabels;

    std::vector<std::wstring> m_availableFenceStyles;
    std::unordered_map<std::wstring, std::wstring> m_fenceStyleLabels;

    std::vector<std::wstring> m_availableMotionPresets;
    std::unordered_map<std::wstring, std::wstring> m_motionPresetLabels;
    std::unordered_map<std::wstring, int> m_motionPresetDurations;
    std::unordered_map<std::wstring, std::wstring> m_motionPresetEasing;

    // Helper to convert nested key paths from resources.json
    std::wstring GetResourceValue(const std::string& keyPath) const;
    std::wstring GetResourceValue(const std::string& keyPath, const std::wstring& defaultValue) const;
    void ParseIconPacksFromResources();
    void ParseButtonFamiliesFromResources();
    void ParseControlFamiliesFromResources();
    void ParseComponentFamiliesFromResources();
    void ParseMenuStylesFromResources();
    void ParseFenceStylesFromResources();
    void ParseMotionPresetsFromResources();
};
