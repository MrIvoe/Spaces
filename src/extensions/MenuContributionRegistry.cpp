#include "extensions/MenuContributionRegistry.h"

#include <algorithm>

void MenuContributionRegistry::Register(const MenuContribution& contribution)
{
    m_contributions.push_back(contribution);
}

std::vector<MenuContribution> MenuContributionRegistry::GetBySurface(MenuSurface surface) const
{
    std::vector<MenuContribution> items;
    for (const auto& contribution : m_contributions)
    {
        if (contribution.surface == surface)
        {
            items.push_back(contribution);
        }
    }

    std::sort(items.begin(), items.end(), [](const MenuContribution& a, const MenuContribution& b) {
        if (a.order == b.order)
        {
            return a.title < b.title;
        }
        return a.order < b.order;
    });

    return items;
}
