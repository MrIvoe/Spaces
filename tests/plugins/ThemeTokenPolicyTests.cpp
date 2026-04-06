#include "PluginTestFixtures.h"

#include <string>

#include "plugins/host/ThemeTokenPolicy.h"

namespace
{
    int Fail(const char* message)
    {
        fprintf(stderr, "[FAIL] %s\n", message);
        return 1;
    }
}

int RunThemeTokenPolicyTests()
{
    using namespace HostPlugins;

    if (!IsValidThemeTokenNamespace(L"win32_theme_system"))
        return Fail("win32_theme_system namespace should be valid");

    if (IsValidThemeTokenNamespace(L"tokens"))
        return Fail("tokens namespace should be invalid");

    if (!IsValidThemeTokenPath(L"host.colors.primary"))
        return Fail("host.colors.primary should be valid");

    if (!IsValidThemeTokenPath(L"theme.surface.default"))
        return Fail("theme.surface.default should be valid");

    if (IsValidThemeTokenPath(L"tokens.colors.primary"))
        return Fail("tokens.* prefix should be invalid");

    if (IsValidThemeTokenPath(L"#FF00FF"))
        return Fail("raw hex color token should be invalid");

    if (IsValidThemeTokenPath(L"host.colors.primary accent"))
        return Fail("token path containing spaces should be invalid");

    return 0;
}
