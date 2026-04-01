#pragma once

#include <string>
#include <vector>

// Field types supported by the settings schema system.
// Plugins declare fields with these types; the host renders the appropriate control.
enum class SettingsFieldType
{
    Bool,   // Rendered as a checkbox. Values: "true" / "false"
    Int,    // Rendered as a single-line numeric edit box. Default as decimal string.
    String, // Rendered as a single-line text edit box.
    Enum,   // Rendered as a combobox (drop-down). Options list must be non-empty.
};

// One option in an Enum field.
struct SettingsEnumOption
{
    std::wstring value; // storage value (used as key)
    std::wstring label; // display text shown in the combobox item
};

// Descriptor for a single interactive settings field declared by a plugin page.
// The host renders the appropriate Win32 control and persists values automatically.
struct SettingsFieldDescriptor
{
    std::wstring key;          // unique storage key, e.g. "appearance.theme.mode"
    std::wstring label;        // short UI label shown left of the control
    std::wstring description;  // sub-label / tooltip text (shown below label when non-empty)
    SettingsFieldType type = SettingsFieldType::Bool;
    std::wstring defaultValue; // "true"/"false" for Bool; decimal string for Int; value string for Enum/String
    std::vector<SettingsEnumOption> options; // only used when type == Enum
    int order = 0;             // smaller values appear first within the page
};
