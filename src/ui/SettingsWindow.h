#pragma once

#include "core/KernelViews.h"

#include <string>
#include <vector>

class SettingsWindow
{
public:
    bool ShowScaffold(const std::vector<SettingsPageView>& pages, const std::vector<PluginStatusView>& plugins) const;

private:
    std::wstring BuildScaffoldText(const std::vector<SettingsPageView>& pages, const std::vector<PluginStatusView>& plugins) const;
};
