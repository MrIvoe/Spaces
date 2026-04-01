#include "core/SettingsStore.h"

#include "Win32Helpers.h"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <windows.h>

// ---------------------------------------------------------------------------
// UTF-8 <-> wstring helpers (local to this TU only)
// ---------------------------------------------------------------------------
namespace
{
    std::string WStringToUtf8(const std::wstring& ws)
    {
        if (ws.empty())
        {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                                             nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            return {};
        }
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                            result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWString(const std::string& s)
    {
        if (s.empty())
        {
            return {};
        }
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                             nullptr, 0);
        if (size <= 0)
        {
            return {};
        }
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                            result.data(), size);
        return result;
    }
} // namespace

// ---------------------------------------------------------------------------
bool SettingsStore::Load(const std::filesystem::path& filePath)
{
    m_filePath = filePath;

    if (!std::filesystem::exists(filePath))
    {
        return false; // first run – no file yet, store starts empty
    }

    try
    {
        std::ifstream ifs(filePath);
        if (!ifs.is_open())
        {
            Win32Helpers::LogError(L"SettingsStore: cannot open " + filePath.wstring());
            return false;
        }

        nlohmann::json root;
        ifs >> root;

        if (!root.is_object())
        {
            Win32Helpers::LogError(L"SettingsStore: unexpected JSON root type");
            return false;
        }

        const auto& values = root["values"];
        if (values.is_object())
        {
            for (auto it = values.begin(); it != values.end(); ++it)
            {
                if (it.value().is_string())
                {
                    m_values[Utf8ToWString(it.key())] = Utf8ToWString(it.value().get<std::string>());
                }
            }
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SettingsStore: JSON parse error: " +
                               Utf8ToWString(ex.what()));
        return false;
    }
}

bool SettingsStore::Save()
{
    if (m_filePath.empty())
    {
        return false;
    }

    try
    {
        nlohmann::json values = nlohmann::json::object();
        for (const auto& [key, val] : m_values)
        {
            values[WStringToUtf8(key)] = WStringToUtf8(val);
        }

        nlohmann::json root;
        root["version"] = 1;
        root["values"]  = values;

        const std::filesystem::path tmpPath = m_filePath.wstring() + L".tmp";
        {
            std::ofstream ofs(tmpPath);
            if (!ofs.is_open())
            {
                Win32Helpers::LogError(L"SettingsStore: cannot write to " + tmpPath.wstring());
                return false;
            }
            ofs << root.dump(2);
        }

        // Atomic replace via Win32Helpers
        return Win32Helpers::ReplaceFileAtomically(tmpPath, m_filePath);
    }
    catch (const std::exception& ex)
    {
        Win32Helpers::LogError(L"SettingsStore: save error: " + Utf8ToWString(ex.what()));
        return false;
    }
}

// ---------------------------------------------------------------------------
std::wstring SettingsStore::Get(const std::wstring& key, const std::wstring& defaultValue) const
{
    const auto it = m_values.find(key);
    return (it != m_values.end()) ? it->second : defaultValue;
}

void SettingsStore::Set(const std::wstring& key, const std::wstring& value)
{
    m_values[key] = value;
    Save();
}

bool SettingsStore::GetBool(const std::wstring& key, bool defaultValue) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
    {
        return defaultValue;
    }
    return it->second == L"true";
}

void SettingsStore::SetBool(const std::wstring& key, bool value)
{
    Set(key, value ? L"true" : L"false");
}

int SettingsStore::GetInt(const std::wstring& key, int defaultValue) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
    {
        return defaultValue;
    }
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return defaultValue;
    }
}

void SettingsStore::SetInt(const std::wstring& key, int value)
{
    Set(key, std::to_wstring(value));
}
