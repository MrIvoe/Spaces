#include "Win32Helpers.h"
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <mutex>

namespace Win32Helpers
{
    namespace
    {
        std::mutex g_logMutex;

        void WriteLogLine(const wchar_t* level, const std::wstring& message)
        {
            std::error_code ec;
            std::filesystem::create_directories(GetAppDataRoot(), ec);

            const auto logPath = GetDebugLogPath();
            std::wofstream log(logPath, std::ios::app);
            if (!log.is_open())
            {
                return;
            }

            SYSTEMTIME st{};
            GetLocalTime(&st);
            log << L"[" << st.wYear << L"-"
                << st.wMonth << L"-"
                << st.wDay << L" "
                << st.wHour << L":"
                << st.wMinute << L":"
                << st.wSecond << L"] "
                << level << L": "
                << message << L"\n";
        }
    }

    std::wstring GetAppDataPath()
    {
        wchar_t path[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return path;
        }
        return L"";
    }

    std::wstring GetLocalAppDataPath()
    {
        wchar_t path[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return path;
        }
        return L"";
    }

    std::filesystem::path GetAppDataRoot()
    {
        return std::filesystem::path(GetLocalAppDataPath()) / L"SimpleFences";
    }

    std::filesystem::path GetFencesRoot()
    {
        return GetAppDataRoot() / L"Fences";
    }

    std::filesystem::path GetConfigPath()
    {
        return GetAppDataRoot() / L"config.json";
    }

    std::filesystem::path GetDebugLogPath()
    {
        return GetAppDataRoot() / L"debug.log";
    }

    bool ReplaceFileAtomically(const std::filesystem::path& tempPath, const std::filesystem::path& targetPath)
    {
        if (MoveFileExW(
                tempPath.c_str(),
                targetPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            return true;
        }

        const DWORD error = GetLastError();
        LogError(
            L"Atomic replace failed from '" + tempPath.wstring() +
            L"' to '" + targetPath.wstring() +
            L"' error=" + std::to_wstring(error));
        return false;
    }

    void LogInfo(const std::wstring& message)
    {
        std::lock_guard<std::mutex> guard(g_logMutex);
        WriteLogLine(L"INFO", message);
    }

    void LogError(const std::wstring& message)
    {
        std::lock_guard<std::mutex> guard(g_logMutex);
        WriteLogLine(L"ERROR", message);
    }

    POINT GetCursorPos()
    {
        POINT pt{};
        ::GetCursorPos(&pt);
        return pt;
    }

    void CenterWindowNearCursor(HWND hwnd)
    {
        POINT pt = GetCursorPos();
        SetWindowPos(hwnd, nullptr, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}
