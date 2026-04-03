#pragma once

#include <string>
#include <vector>

enum class MenuSurface
{
    Tray,
    FenceContext,
    ItemContext,
    DesktopContext,
    SettingsNavigation
};

struct MenuContribution
{
    MenuSurface surface = MenuSurface::Tray;
    std::wstring title;
    std::wstring commandId;
    int order = 0;
    bool separatorBefore = false;
};

class MenuContributionRegistry
{
public:
    bool Register(const MenuContribution& contribution);
    std::vector<MenuContribution> GetBySurface(MenuSurface surface) const;
    void Clear();

private:
    std::vector<MenuContribution> m_contributions;
};
