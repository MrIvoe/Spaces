#include "core/Diagnostics.h"
#include "core/SettingsStore.h"
#include "Win32Helpers.h"

void Diagnostics::SetStore(SettingsStore* store)
{
    m_store = store;
}

bool Diagnostics::IsEnabled() const
{
    if (!m_store)
    {
        return true;
    }

    return m_store->GetBool(L"settings.diagnostics.logging_enabled", true);
}

Diagnostics::Level Diagnostics::GetLevel() const
{
    if (!m_store)
    {
        return Level::Info;
    }

    const std::wstring value = m_store->Get(L"settings.diagnostics.log_level", L"info");
    if (value == L"error")
    {
        return Level::Error;
    }
    if (value == L"warn")
    {
        return Level::Warn;
    }
    if (value == L"debug")
    {
        return Level::Debug;
    }

    return Level::Info;
}

void Diagnostics::Info(const std::wstring& message) const
{
    if (!IsEnabled() || static_cast<int>(GetLevel()) < static_cast<int>(Level::Info))
    {
        return;
    }

    Win32Helpers::LogInfo(message);
}

void Diagnostics::Warn(const std::wstring& message) const
{
    if (!IsEnabled() || static_cast<int>(GetLevel()) < static_cast<int>(Level::Warn))
    {
        return;
    }

    Win32Helpers::LogInfo(L"[WARN] " + message);
}

void Diagnostics::Error(const std::wstring& message) const
{
    if (!IsEnabled() || static_cast<int>(GetLevel()) < static_cast<int>(Level::Error))
    {
        return;
    }

    Win32Helpers::LogError(message);
}
