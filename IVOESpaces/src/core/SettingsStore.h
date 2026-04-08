#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/// Persistent JSON-based settings store
class SettingsStore {
public:
    SettingsStore();
    ~SettingsStore();

    /// Initialize settings store
    bool Initialize(const std::wstring& configPath);

    /// Read string setting
    bool ReadString(const std::wstring& key, std::wstring& outValue);

    /// Write string setting
    bool WriteString(const std::wstring& key, const std::wstring& value);

    /// Read integer setting
    bool ReadInt(const std::wstring& key, int32_t& outValue);

    /// Write integer setting
    bool WriteInt(const std::wstring& key, int32_t value);

    /// Read float setting
    bool ReadFloat(const std::wstring& key, float& outValue);

    /// Write float setting
    bool WriteFloat(const std::wstring& key, float value);

    /// Delete a setting
    bool DeleteKey(const std::wstring& key);

    /// Save all settings to disk
    bool Save();

    /// Load all settings from disk
    bool Load();

private:
    std::unordered_map<std::wstring, std::wstring> m_settings;
    std::wstring m_configPath;
};
