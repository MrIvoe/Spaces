#include "core/ThemeResourceResolver.h"
#include "core/SettingsStore.h"
#include "core/UniversalThemeLoader.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <cctype>

namespace {
    std::wstring Utf8ToWString(const std::string& s) {
        if (s.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (size <= 0) return {};
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), size);
        return out;
    }

    std::string WStringToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    template <typename T>
    void EnsureContains(std::vector<T>& list, const T& value)
    {
        for (const auto& existing : list)
        {
            if (existing == value)
            {
                return;
            }
        }
        list.push_back(value);
    }

    void EnsureLabel(std::unordered_map<std::wstring, std::wstring>& labels,
                     const std::wstring& key,
                     const std::wstring& label)
    {
        if (labels.find(key) == labels.end())
        {
            labels[key] = label;
        }
    }

    bool ContainsAny(const std::string& value, std::initializer_list<const char*> needles)
    {
        for (const char* needle : needles)
        {
            if (value.find(needle) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }
}

ThemeResourceResolver::ThemeResourceResolver(SettingsStore* settingsStore)
    : m_settingsStore(settingsStore)
{
}

void ThemeResourceResolver::Initialize(const UniversalThemeData* themeData)
{
    m_themeData = themeData;
    if (m_themeData)
    {
        ParseIconPacksFromResources();
        ParseButtonFamiliesFromResources();
        ParseControlFamiliesFromResources();
        ParseComponentFamiliesFromResources();
        ParseMenuStylesFromResources();
        ParseFenceStylesFromResources();
        ParseMotionPresetsFromResources();
    }

    // Always keep usable defaults so UI theming still works even when resources.json
    // is unavailable for a given theme source.
    EnsureContains(m_availableIconPacks, std::wstring(L"lucide"));
    EnsureContains(m_availableIconPacks, std::wstring(L"heroicons"));
    EnsureContains(m_availableIconPacks, std::wstring(L"tabler"));
    EnsureContains(m_availableIconPacks, std::wstring(L"bootstrap-icons"));
    EnsureLabel(m_iconPackLabels, L"lucide", L"Lucide");
    EnsureLabel(m_iconPackLabels, L"heroicons", L"Heroicons");
    EnsureLabel(m_iconPackLabels, L"tabler", L"Tabler");
    EnsureLabel(m_iconPackLabels, L"bootstrap-icons", L"Bootstrap Icons");

    EnsureContains(m_availableButtonFamilies, std::wstring(L"compact"));
    EnsureContains(m_availableButtonFamilies, std::wstring(L"soft"));
    EnsureContains(m_availableButtonFamilies, std::wstring(L"outlined"));
    EnsureContains(m_availableButtonFamilies, std::wstring(L"high-contrast"));
    EnsureLabel(m_buttonFamilyLabels, L"compact", L"Compact");
    EnsureLabel(m_buttonFamilyLabels, L"soft", L"Soft");
    EnsureLabel(m_buttonFamilyLabels, L"outlined", L"Outlined");
    EnsureLabel(m_buttonFamilyLabels, L"high-contrast", L"High Contrast");

    EnsureContains(m_availableControlFamilies, std::wstring(L"desktop-fluent"));
    EnsureContains(m_availableControlFamilies, std::wstring(L"dashboard-modern"));
    EnsureContains(m_availableControlFamilies, std::wstring(L"flat-minimal"));
    EnsureContains(m_availableControlFamilies, std::wstring(L"soft-mobile"));
    EnsureContains(m_availableControlFamilies, std::wstring(L"cyber-futuristic"));

    EnsureContains(m_availableComponentFamilies, std::wstring(L"desktop-fluent"));
    EnsureContains(m_availableComponentFamilies, std::wstring(L"dashboard-modern"));
    EnsureContains(m_availableComponentFamilies, std::wstring(L"flat-minimal"));
    EnsureContains(m_availableComponentFamilies, std::wstring(L"soft-mobile"));
    EnsureContains(m_availableComponentFamilies, std::wstring(L"cyber-futuristic"));
    EnsureLabel(m_componentFamilyLabels, L"desktop-fluent", L"Desktop Fluent");
    EnsureLabel(m_componentFamilyLabels, L"dashboard-modern", L"Dashboard Modern");
    EnsureLabel(m_componentFamilyLabels, L"flat-minimal", L"Flat Minimal");
    EnsureLabel(m_componentFamilyLabels, L"soft-mobile", L"Soft Mobile");
    EnsureLabel(m_componentFamilyLabels, L"cyber-futuristic", L"Cyber Futuristic");

    EnsureContains(m_availableMenuStyles, std::wstring(L"standard"));
    EnsureContains(m_availableMenuStyles, std::wstring(L"compact"));
    EnsureContains(m_availableMenuStyles, std::wstring(L"hierarchical"));
    EnsureLabel(m_menuStyleLabels, L"standard", L"Standard");
    EnsureLabel(m_menuStyleLabels, L"compact", L"Compact");
    EnsureLabel(m_menuStyleLabels, L"hierarchical", L"Hierarchical");

    EnsureContains(m_availableFenceStyles, std::wstring(L"window-frame"));
    EnsureContains(m_availableFenceStyles, std::wstring(L"card-container"));
    EnsureContains(m_availableFenceStyles, std::wstring(L"embedded"));
    EnsureLabel(m_fenceStyleLabels, L"window-frame", L"Window Frame");
    EnsureLabel(m_fenceStyleLabels, L"card-container", L"Card Container");
    EnsureLabel(m_fenceStyleLabels, L"embedded", L"Embedded");

    EnsureContains(m_availableMotionPresets, std::wstring(L"standard"));
    EnsureContains(m_availableMotionPresets, std::wstring(L"quick"));
    EnsureContains(m_availableMotionPresets, std::wstring(L"deliberate"));
    EnsureContains(m_availableMotionPresets, std::wstring(L"minimal"));
    EnsureLabel(m_motionPresetLabels, L"standard", L"Standard");
    EnsureLabel(m_motionPresetLabels, L"quick", L"Quick");
    EnsureLabel(m_motionPresetLabels, L"deliberate", L"Deliberate");
    EnsureLabel(m_motionPresetLabels, L"minimal", L"Minimal");
    if (m_motionPresetDurations.find(L"standard") == m_motionPresetDurations.end()) m_motionPresetDurations[L"standard"] = 400;
    if (m_motionPresetDurations.find(L"quick") == m_motionPresetDurations.end()) m_motionPresetDurations[L"quick"] = 200;
    if (m_motionPresetDurations.find(L"deliberate") == m_motionPresetDurations.end()) m_motionPresetDurations[L"deliberate"] = 600;
    if (m_motionPresetDurations.find(L"minimal") == m_motionPresetDurations.end()) m_motionPresetDurations[L"minimal"] = 0;
}

std::wstring ThemeResourceResolver::GetResourceValue(const std::string& keyPath) const
{
    if (!m_themeData)
    {
        return {};
    }

    const auto it = m_themeData->resources.find(keyPath);
    if (it != m_themeData->resources.end())
    {
        return Utf8ToWString(it->second);
    }

    return {};
}

std::wstring ThemeResourceResolver::GetResourceValue(const std::string& keyPath, const std::wstring& defaultValue) const
{
    const std::wstring result = GetResourceValue(keyPath);
    return result.empty() ? defaultValue : result;
}

void ThemeResourceResolver::ParseIconPacksFromResources()
{
    m_availableIconPacks.clear();
    m_iconPackLabels.clear();

    if (!m_themeData)
    {
        return;
    }

    // Look for ui.icons.availablePacks array
    for (const auto& [key, value] : m_themeData->resources)
    {
        // Match pattern: ui.icons.availablePacks[0], ui.icons.availablePacks[1], etc.
        if (key.find("ui.icons.availablePacks") != std::string::npos)
        {
            const std::wstring packId = Utf8ToWString(value);
            m_availableIconPacks.push_back(packId);
        }

        // Parse icon pack labels: ui.icons.packs.<packId>.label
        if (key.find("ui.icons.packs.") != std::string::npos && key.find(".label") != std::string::npos)
        {
            // Extract pack ID from key like "ui.icons.packs.lucide.label"
            const size_t start = key.find("packs.") + 6;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string packId = key.substr(start, end - start);
                m_iconPackLabels[Utf8ToWString(packId)] = Utf8ToWString(value);
            }
        }
    }
}

void ThemeResourceResolver::ParseButtonFamiliesFromResources()
{
    m_availableButtonFamilies.clear();
    m_buttonFamilyLabels.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        // Match pattern: ui.buttons.availableFamilies[0], etc.
        if (key.find("ui.buttons.availableFamilies") != std::string::npos)
        {
            const std::wstring familyId = Utf8ToWString(value);
            m_availableButtonFamilies.push_back(familyId);
        }

        // Parse button family labels: ui.buttons.families.<familyId>.label
        if (key.find("ui.buttons.families.") != std::string::npos && key.find(".label") != std::string::npos)
        {
            const size_t start = key.find("families.") + 9;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string familyId = key.substr(start, end - start);
                m_buttonFamilyLabels[Utf8ToWString(familyId)] = Utf8ToWString(value);
            }
        }
    }
}

void ThemeResourceResolver::ParseControlFamiliesFromResources()
{
    m_availableControlFamilies.clear();
    m_controlFamilyLabels.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        // Support both legacy and current keys:
        // - ui.controls.availableFamilies[*]
        // - ui.controlFamilies.availableFamilies[*]
        if (ContainsAny(key, {"ui.controls.availableFamilies", "ui.controlFamilies.availableFamilies"}))
        {
            const std::wstring familyId = Utf8ToWString(value);
            m_availableControlFamilies.push_back(familyId);
        }

        // Support both legacy and current label key paths:
        // - ui.controls.families.<familyId>.label
        // - ui.controlFamilies.families.<familyId>.label
        if (key.find(".label") != std::string::npos &&
            ContainsAny(key, {"ui.controls.families.", "ui.controlFamilies.families."}))
        {
            const size_t start = key.find("families.") + 9;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string familyId = key.substr(start, end - start);
                m_controlFamilyLabels[Utf8ToWString(familyId)] = Utf8ToWString(value);
            }
        }
    }
}

void ThemeResourceResolver::ParseComponentFamiliesFromResources()
{
    m_availableComponentFamilies.clear();
    m_componentFamilyLabels.clear();
    m_componentFamilyDescriptions.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        // Match pattern: ui.componentFamily.available[0], etc.
        if (key.find("ui.componentFamily.available") != std::string::npos)
        {
            const std::wstring familyId = Utf8ToWString(value);
            m_availableComponentFamilies.push_back(familyId);
        }

        // Parse component family labels: ui.<familyId>.label
        // where familyId is desktop-fluent, dashboard-modern, etc.
        if (key.find(".label") != std::string::npos && 
            (key.find("desktop-fluent") != std::string::npos ||
             key.find("dashboard-modern") != std::string::npos ||
             key.find("flat-minimal") != std::string::npos ||
             key.find("soft-mobile") != std::string::npos ||
             key.find("cyber-futuristic") != std::string::npos))
        {
            // Extract family ID from pattern like "ui.desktop-fluent.label"
            if (key.find("ui.") == 0)
            {
                const size_t start = 3;
                const size_t end = key.find(".label");
                if (end != std::string::npos && start < end)
                {
                    const std::string familyId = key.substr(start, end - start);
                    m_componentFamilyLabels[Utf8ToWString(familyId)] = Utf8ToWString(value);
                }
            }
        }

        // Parse descriptions: ui.<familyId>.description
        if (key.find(".description") != std::string::npos && 
            (key.find("desktop-fluent") != std::string::npos ||
             key.find("dashboard-modern") != std::string::npos ||
             key.find("flat-minimal") != std::string::npos ||
             key.find("soft-mobile") != std::string::npos ||
             key.find("cyber-futuristic") != std::string::npos))
        {
            if (key.find("ui.") == 0)
            {
                const size_t start = 3;
                const size_t end = key.find(".description");
                if (end != std::string::npos && start < end)
                {
                    const std::string familyId = key.substr(start, end - start);
                    m_componentFamilyDescriptions[Utf8ToWString(familyId)] = Utf8ToWString(value);
                }
            }
        }
    }
}

void ThemeResourceResolver::ParseMenuStylesFromResources()
{
    m_availableMenuStyles.clear();
    m_menuStyleLabels.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        if (key.find("ui.menus.availableStyles") != std::string::npos)
        {
            const std::wstring styleId = Utf8ToWString(value);
            m_availableMenuStyles.push_back(styleId);
        }

        if (key.find("ui.menus.styles.") != std::string::npos && key.find(".label") != std::string::npos)
        {
            const size_t start = key.find("styles.") + 7;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string styleId = key.substr(start, end - start);
                m_menuStyleLabels[Utf8ToWString(styleId)] = Utf8ToWString(value);
            }
        }
    }
}

void ThemeResourceResolver::ParseFenceStylesFromResources()
{
    m_availableFenceStyles.clear();
    m_fenceStyleLabels.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        if (key.find("ui.fences.availableStyles") != std::string::npos)
        {
            const std::wstring styleId = Utf8ToWString(value);
            m_availableFenceStyles.push_back(styleId);
        }

        if (key.find("ui.fences.styles.") != std::string::npos && key.find(".label") != std::string::npos)
        {
            const size_t start = key.find("styles.") + 7;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string styleId = key.substr(start, end - start);
                m_fenceStyleLabels[Utf8ToWString(styleId)] = Utf8ToWString(value);
            }
        }
    }
}

void ThemeResourceResolver::ParseMotionPresetsFromResources()
{
    m_availableMotionPresets.clear();
    m_motionPresetLabels.clear();
    m_motionPresetDurations.clear();
    m_motionPresetEasing.clear();

    if (!m_themeData)
    {
        return;
    }

    for (const auto& [key, value] : m_themeData->resources)
    {
        if (key.find("ui.motion.availablePresets") != std::string::npos)
        {
            const std::wstring presetId = Utf8ToWString(value);
            m_availableMotionPresets.push_back(presetId);
        }

        if (key.find("ui.motion.presets.") != std::string::npos && key.find(".label") != std::string::npos)
        {
            const size_t start = key.find("presets.") + 8;
            const size_t end = key.find(".label");
            if (start < end)
            {
                const std::string presetId = key.substr(start, end - start);
                m_motionPresetLabels[Utf8ToWString(presetId)] = Utf8ToWString(value);
            }
        }

        if (key.find("ui.motion.presets.") != std::string::npos && key.find(".duration") != std::string::npos)
        {
            const size_t start = key.find("presets.") + 8;
            const size_t end = key.find(".duration");
            if (start < end)
            {
                const std::string presetId = key.substr(start, end - start);
                try
                {
                    m_motionPresetDurations[Utf8ToWString(presetId)] = std::stoi(value);
                }
                catch (...)
                {
                }
            }
        }

        if (key.find("ui.motion.presets.") != std::string::npos && key.find(".easing") != std::string::npos)
        {
            const size_t start = key.find("presets.") + 8;
            const size_t end = key.find(".easing");
            if (start < end)
            {
                const std::string presetId = key.substr(start, end - start);
                m_motionPresetEasing[Utf8ToWString(presetId)] = Utf8ToWString(value);
            }
        }
    }
}

// Icon Pack Resolution ====================================================

std::wstring ThemeResourceResolver::GetDefaultIconPack() const
{
    return GetResourceValue("ui.icons.defaultPack", L"lucide");
}

std::wstring ThemeResourceResolver::GetSelectedIconPack() const
{
    if (!m_settingsStore)
    {
        return GetDefaultIconPack();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.icon_pack", L"");
    if (!selected.empty() && IsIconPackAvailable(selected))
    {
        return selected;
    }

    return GetDefaultIconPack();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableIconPacks() const
{
    return m_availableIconPacks;
}

std::wstring ThemeResourceResolver::GetIconPackLabel(const std::wstring& packId) const
{
    const auto it = m_iconPackLabels.find(packId);
    return it != m_iconPackLabels.end() ? it->second : packId;
}

std::wstring ThemeResourceResolver::GetIconPackDescription(const std::wstring& packId) const
{
    std::string keyPath = "ui.icons.packs." + WStringToUtf8(packId) + ".description";
    return GetResourceValue(keyPath);
}

// Button Family Resolution ================================================

std::wstring ThemeResourceResolver::GetDefaultButtonFamily() const
{
    return GetResourceValue("ui.buttons.defaultFamily", L"compact");
}

std::wstring ThemeResourceResolver::GetSelectedButtonFamily() const
{
    if (!m_settingsStore)
    {
        return GetDefaultButtonFamily();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.button_family", L"");
    if (!selected.empty() && IsButtonFamilyAvailable(selected))
    {
        return selected;
    }

    return GetDefaultButtonFamily();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableButtonFamilies() const
{
    return m_availableButtonFamilies;
}

std::wstring ThemeResourceResolver::GetButtonFamilyLabel(const std::wstring& familyId) const
{
    const auto it = m_buttonFamilyLabels.find(familyId);
    return it != m_buttonFamilyLabels.end() ? it->second : familyId;
}

// Control Family Resolution ===============================================

std::wstring ThemeResourceResolver::GetDefaultControlFamily() const
{
    const std::wstring current = GetResourceValue("ui.controlFamilies.defaultFamily", L"");
    if (!current.empty())
    {
        return current;
    }
    return GetResourceValue("ui.controls.defaultFamily", L"desktop-fluent");
}

std::wstring ThemeResourceResolver::GetSelectedControlFamily() const
{
    if (!m_settingsStore)
    {
        return GetDefaultControlFamily();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.controls_family", L"");
    if (!selected.empty() && IsControlFamilyAvailable(selected))
    {
        return selected;
    }

    return GetDefaultControlFamily();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableControlFamilies() const
{
    return m_availableControlFamilies;
}

std::wstring ThemeResourceResolver::GetControlFamilyLabel(const std::wstring& familyId) const
{
    const auto it = m_controlFamilyLabels.find(familyId);
    return it != m_controlFamilyLabels.end() ? it->second : familyId;
}

// Component Family Resolution =============================================

std::wstring ThemeResourceResolver::GetDefaultComponentFamily() const
{
    return GetResourceValue("ui.componentFamily.default", L"desktop-fluent");
}

std::wstring ThemeResourceResolver::GetSelectedComponentFamily() const
{
    if (!m_settingsStore)
    {
        return GetDefaultComponentFamily();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.component_family", L"");
    if (!selected.empty() && IsComponentFamilyAvailable(selected))
    {
        return selected;
    }

    return GetDefaultComponentFamily();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableComponentFamilies() const
{
    return m_availableComponentFamilies;
}

std::wstring ThemeResourceResolver::GetComponentFamilyLabel(const std::wstring& familyId) const
{
    const auto it = m_componentFamilyLabels.find(familyId);
    return it != m_componentFamilyLabels.end() ? it->second : familyId;
}

std::wstring ThemeResourceResolver::GetComponentFamilyDescription(const std::wstring& familyId) const
{
    const auto it = m_componentFamilyDescriptions.find(familyId);
    return it != m_componentFamilyDescriptions.end() ? it->second : L"";
}

// Menu Style Resolution ===================================================

std::wstring ThemeResourceResolver::GetDefaultMenuStyle() const
{
    return GetResourceValue("ui.menus.defaultStyle", L"standard");
}

std::wstring ThemeResourceResolver::GetSelectedMenuStyle() const
{
    if (!m_settingsStore)
    {
        return GetDefaultMenuStyle();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.menu_style", L"");
    if (!selected.empty())
    {
        return selected;
    }

    return GetDefaultMenuStyle();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableMenuStyles() const
{
    return m_availableMenuStyles;
}

std::wstring ThemeResourceResolver::GetMenuStyleLabel(const std::wstring& styleId) const
{
    const auto it = m_menuStyleLabels.find(styleId);
    return it != m_menuStyleLabels.end() ? it->second : styleId;
}

// Fence Shell Resolution ==================================================

std::wstring ThemeResourceResolver::GetDefaultFenceStyle() const
{
    return GetResourceValue("ui.fences.defaultStyle", L"window-frame");
}

std::wstring ThemeResourceResolver::GetSelectedFenceStyle() const
{
    if (!m_settingsStore)
    {
        return GetDefaultFenceStyle();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.fence_style", L"");
    if (!selected.empty())
    {
        return selected;
    }

    return GetDefaultFenceStyle();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableFenceStyles() const
{
    return m_availableFenceStyles;
}

std::wstring ThemeResourceResolver::GetFenceStyleLabel(const std::wstring& styleId) const
{
    const auto it = m_fenceStyleLabels.find(styleId);
    return it != m_fenceStyleLabels.end() ? it->second : styleId;
}

// Motion Preset Resolution ================================================

std::wstring ThemeResourceResolver::GetDefaultMotionPreset() const
{
    return GetResourceValue("ui.motion.defaultPreset", L"standard");
}

std::wstring ThemeResourceResolver::GetSelectedMotionPreset() const
{
    if (!m_settingsStore)
    {
        return GetDefaultMotionPreset();
    }

    const std::wstring selected = m_settingsStore->Get(L"appearance.ui.motion_preset", L"");
    if (!selected.empty())
    {
        return selected;
    }

    return GetDefaultMotionPreset();
}

std::vector<std::wstring> ThemeResourceResolver::GetAvailableMotionPresets() const
{
    return m_availableMotionPresets;
}

std::wstring ThemeResourceResolver::GetMotionPresetLabel(const std::wstring& presetId) const
{
    const auto it = m_motionPresetLabels.find(presetId);
    return it != m_motionPresetLabels.end() ? it->second : presetId;
}

int ThemeResourceResolver::GetMotionDurationMs(const std::wstring& presetId, int fallbackMs) const
{
    const auto it = m_motionPresetDurations.find(presetId);
    return it != m_motionPresetDurations.end() ? it->second : fallbackMs;
}

std::wstring ThemeResourceResolver::GetMotionEasing(const std::wstring& presetId) const
{
    const auto it = m_motionPresetEasing.find(presetId);
    return it != m_motionPresetEasing.end() ? it->second : L"cubic-bezier(0.25, 0.46, 0.45, 0.94)";
}

// Settings Persistence ===================================================

void ThemeResourceResolver::SetIconPack(const std::wstring& packId)
{
    if (m_settingsStore && IsIconPackAvailable(packId))
    {
        m_settingsStore->Set(L"appearance.ui.icon_pack", packId);
    }
}

void ThemeResourceResolver::SetButtonFamily(const std::wstring& familyId)
{
    if (m_settingsStore && IsButtonFamilyAvailable(familyId))
    {
        m_settingsStore->Set(L"appearance.ui.button_family", familyId);
    }
}

void ThemeResourceResolver::SetControlFamily(const std::wstring& familyId)
{
    if (m_settingsStore && IsControlFamilyAvailable(familyId))
    {
        m_settingsStore->Set(L"appearance.ui.controls_family", familyId);
    }
}

void ThemeResourceResolver::SetComponentFamily(const std::wstring& familyId)
{
    if (m_settingsStore && IsComponentFamilyAvailable(familyId))
    {
        m_settingsStore->Set(L"appearance.ui.component_family", familyId);
    }
}

void ThemeResourceResolver::SetMenuStyle(const std::wstring& styleId)
{
    if (m_settingsStore)
    {
        m_settingsStore->Set(L"appearance.ui.menu_style", styleId);
    }
}

void ThemeResourceResolver::SetFenceStyle(const std::wstring& styleId)
{
    if (m_settingsStore)
    {
        m_settingsStore->Set(L"appearance.ui.fence_style", styleId);
    }
}

void ThemeResourceResolver::SetMotionPreset(const std::wstring& presetId)
{
    if (m_settingsStore)
    {
        m_settingsStore->Set(L"appearance.ui.motion_preset", presetId);
    }
}

// Query Helpers ===========================================================

bool ThemeResourceResolver::IsIconPackAvailable(const std::wstring& packId) const
{
    if (packId.empty())
    {
        return false;
    }

    if (m_availableIconPacks.empty())
    {
        return true;
    }

    for (const auto& pack : m_availableIconPacks)
    {
        if (pack == packId)
        {
            return true;
        }
    }
    return false;
}

bool ThemeResourceResolver::IsButtonFamilyAvailable(const std::wstring& familyId) const
{
    if (familyId.empty())
    {
        return false;
    }

    if (m_availableButtonFamilies.empty())
    {
        return true;
    }

    for (const auto& family : m_availableButtonFamilies)
    {
        if (family == familyId)
        {
            return true;
        }
    }
    return false;
}

bool ThemeResourceResolver::IsControlFamilyAvailable(const std::wstring& familyId) const
{
    if (familyId.empty())
    {
        return false;
    }

    if (m_availableControlFamilies.empty())
    {
        return true;
    }

    for (const auto& family : m_availableControlFamilies)
    {
        if (family == familyId)
        {
            return true;
        }
    }
    return false;
}

bool ThemeResourceResolver::IsComponentFamilyAvailable(const std::wstring& familyId) const
{
    if (familyId.empty())
    {
        return false;
    }

    if (m_availableComponentFamilies.empty())
    {
        return true;
    }

    for (const auto& family : m_availableComponentFamilies)
    {
        if (family == familyId)
        {
            return true;
        }
    }
    return false;
}

size_t ThemeResourceResolver::GetIconPackCount() const
{
    return m_availableIconPacks.size();
}

size_t ThemeResourceResolver::GetComponentFamilyCount() const
{
    return m_availableComponentFamilies.size();
}
