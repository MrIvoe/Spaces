#pragma once

#include <string>

namespace HostPlugins
{
    inline bool IsValidThemeTokenNamespace(const std::wstring& tokenNamespace)
    {
        return tokenNamespace == L"win32_theme_system";
    }

    inline bool IsValidThemeTokenPath(const std::wstring& tokenPath)
    {
        if (tokenPath.empty())
            return false;

        if (tokenPath[0] == L'#')
            return false;

        const bool hostToken = tokenPath.rfind(L"host.", 0) == 0;
        const bool themeToken = tokenPath.rfind(L"theme.", 0) == 0;
        if (!hostToken && !themeToken)
            return false;

        return tokenPath.find(L" ") == std::wstring::npos;
    }
}
