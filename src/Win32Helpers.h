#pragma once
#include <windows.h>
#include <string>

namespace Win32Helpers
{
    std::wstring GetAppDataPath();
    std::wstring GetLocalAppDataPath();
    POINT GetCursorPos();
    void CenterWindowNearCursor(HWND hwnd);
}
