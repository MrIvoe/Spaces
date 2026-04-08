#pragma once

#include <string>

class SettingsStore;

class Diagnostics
{
public:
    void SetStore(SettingsStore* store);

    void Info(const std::wstring& message) const;
    void Warn(const std::wstring& message) const;
    void Error(const std::wstring& message) const;

private:
    enum class Level
    {
        Error = 0,
        Warn = 1,
        Info = 2,
        Debug = 3
    };

    bool IsEnabled() const;
    Level GetLevel() const;

    SettingsStore* m_store = nullptr;
};
