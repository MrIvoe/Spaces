using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.AI;

internal sealed class GroupingEngine
{
    public IReadOnlyList<FenceIconGroupingSuggester.GroupingSuggestion> Suggest(FenceItemModel item)
    {
        return FenceIconGroupingSuggester.Instance.GetFenceSuggestions(item, FenceStateService.Instance.Fences, 3);
    }

    public void ApplySmartSort(FenceModel fence)
    {
        List<Guid> order = FenceIconGroupingSuggester.Instance.GetSmartSortOrder(fence);
        if (order.Count == 0)
            return;

        fence.Items = fence.Items
            .OrderBy(i => order.IndexOf(i.Id) < 0 ? int.MaxValue : order.IndexOf(i.Id))
            .ToList();
    }
}
