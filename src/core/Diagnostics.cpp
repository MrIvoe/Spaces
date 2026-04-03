#include "core/Diagnostics.h"
#include "Win32Helpers.h"

void Diagnostics::Info(const std::wstring& message) const
{
    Win32Helpers::LogInfo(message);
}

void Diagnostics::Warn(const std::wstring& message) const
{
    Win32Helpers::LogInfo(L"[WARN] " + message);
}

void Diagnostics::Error(const std::wstring& message) const
{
    Win32Helpers::LogError(message);
}
