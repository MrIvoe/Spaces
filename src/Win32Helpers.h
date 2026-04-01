#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace Win32Helpers
{
    std::wstring GetAppDataPath();
    std::wstring GetLocalAppDataPath();
    std::filesystem::path GetAppDataRoot();
    std::filesystem::path GetFencesRoot();
    std::filesystem::path GetConfigPath();
    std::filesystem::path GetDebugLogPath();

    void LogInfo(const std::wstring& message);
    void LogError(const std::wstring& message);

    POINT GetCursorPos();
    void CenterWindowNearCursor(HWND hwnd);
}
