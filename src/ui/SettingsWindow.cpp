#include "ui/SettingsWindow.h"

#include <windows.h>

namespace
{
    std::wstring JoinCapabilities(const std::vector<std::wstring>& capabilities)
    {
        if (capabilities.empty())
        {
            return L"none";
        }

        std::wstring text;
        for (size_t i = 0; i < capabilities.size(); ++i)
        {
            text += capabilities[i];
            if (i + 1 < capabilities.size())
            {
                text += L", ";
            }
        }

        return text;
    }
}

bool SettingsWindow::ShowScaffold(const std::vector<SettingsPageView>& pages, const std::vector<PluginStatusView>& plugins) const
{
    const std::wstring text = BuildScaffoldText(pages, plugins);
    const int result = MessageBoxW(
        nullptr,
        text.c_str(),
        L"SimpleFences Settings (0.0.010 Scaffold)",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);

    return result == IDOK;
}

std::wstring SettingsWindow::BuildScaffoldText(const std::vector<SettingsPageView>& pages, const std::vector<PluginStatusView>& plugins) const
{
    std::wstring text;

    text += L"General\r\n";
    text += L"- Settings host shell is active.\r\n";
    text += L"- Core fences remain first-class and stable.\r\n\r\n";

    text += L"Pages\r\n";
    for (const auto& page : pages)
    {
        text += L"- [" + page.pluginId + L"] " + page.title + L" (" + page.pageId + L")\r\n";
    }

    if (pages.empty())
    {
        text += L"- No pages registered.\r\n";
    }

    text += L"\r\nPlugins\r\n";
    for (const auto& plugin : plugins)
    {
        text += L"- " + plugin.displayName + L" (" + plugin.id + L", v" + plugin.version + L")\r\n";
        text += L"  state: ";
        if (!plugin.enabled)
        {
            text += L"disabled";
        }
        else
        {
            text += plugin.loaded ? L"loaded" : L"failed";
        }
        text += L"\r\n";

        text += L"  capabilities: " + JoinCapabilities(plugin.capabilities) + L"\r\n";
        if (!plugin.lastError.empty())
        {
            text += L"  error: " + plugin.lastError + L"\r\n";
        }
    }

    if (plugins.empty())
    {
        text += L"- No plugins registered.\r\n";
    }

    text += L"\r\nDiagnostics\r\n";
    text += L"- This shell surfaces plugin status and capabilities.\r\n";
    text += L"- Rich page UI will replace this scaffold in a later iteration.\r\n";

    return text;
}
