#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

// Lightweight JSON-backed key/value store persisted to
// %LOCALAPPDATA%\SimpleFences\settings.json.
//
// All values are stored as wstrings internally and serialised as UTF-8 JSON.
// The store is loaded once on startup and saved on every Set() call.
class SettingsStore
{
public:
    // Load values from filePath.  Returns false if the file does not exist yet
    // (not an error – the store starts empty and will be written on first Set()).
    bool Load(const std::filesystem::path& filePath);

    // Persist current values to the file supplied at Load time.
    bool Save();

    // Generic string accessors -----------------------------------------
    std::wstring Get(const std::wstring& key, const std::wstring& defaultValue) const;
    void         Set(const std::wstring& key, const std::wstring& value);

    // Typed convenience helpers -----------------------------------------
    bool GetBool(const std::wstring& key, bool defaultValue) const;
    void SetBool(const std::wstring& key, bool value);

    int  GetInt(const std::wstring& key, int defaultValue) const;
    void SetInt(const std::wstring& key, int value);

private:
    std::filesystem::path                        m_filePath;
    std::unordered_map<std::wstring, std::wstring> m_values;
};
