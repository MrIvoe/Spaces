using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.AI;

internal sealed class GroupingEngine
{
    public IReadOnlyList<SpaceIconGroupingSuggester.GroupingSuggestion> Suggest(SpaceItemModel item)
    {
        return SpaceIconGroupingSuggester.Instance.GetSpaceSuggestions(item, SpaceStateService.Instance.Spaces, 3);
    }

    public void ApplySmartSort(SpaceModel space)
    {
        List<Guid> order = SpaceIconGroupingSuggester.Instance.GetSmartSortOrder(space);
        if (order.Count == 0)
            return;

        space.Items = space.Items
            .OrderBy(i => order.IndexOf(i.Id) < 0 ? int.MaxValue : order.IndexOf(i.Id))
            .ToList();
    }
}
