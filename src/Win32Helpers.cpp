#include "Win32Helpers.h"
#include <shlobj.h>
#include <codecvt>

namespace Win32Helpers
{
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
