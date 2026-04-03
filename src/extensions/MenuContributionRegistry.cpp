#include "extensions/MenuContributionRegistry.h"

#include <algorithm>

bool MenuContributionRegistry::Register(const MenuContribution& contribution)
{
    if (contribution.title.empty() || contribution.commandId.empty())
    {
        return false;
    }

    for (const auto& existing : m_contributions)
    {
        if (existing.surface == contribution.surface &&
            existing.title == contribution.title &&
            existing.commandId == contribution.commandId)
        {
            return false;
        }
    }

    m_contributions.push_back(contribution);
    return true;
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

void MenuContributionRegistry::Clear()
{
    m_contributions.clear();
}
