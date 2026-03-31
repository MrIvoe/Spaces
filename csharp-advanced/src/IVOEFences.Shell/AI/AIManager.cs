using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.AI;

internal sealed class AIManager
{
    private readonly IconClassifier _classifier = new();
    private readonly GroupingEngine _grouping = new();

    public IReadOnlyList<FenceIconGroupingSuggester.GroupingSuggestion> AnalyzeAndSuggest(FenceItemModel item)
    {
        _classifier.Enrich(item);
        var suggestions = _grouping.Suggest(item);

        if (suggestions.Count > 0)
        {
            item.LastSuggestedMoveTime = DateTime.UtcNow;
            Log.Information("AIManager: top suggestion for {Item} is {Fence} ({Score:0.00})",
                item.DisplayName,
                suggestions[0].Fence.Title,
                suggestions[0].ConfidenceScore);
        }

        return suggestions;
    }

    public void SmartGroupFence(FenceModel fence)
    {
        foreach (FenceItemModel item in fence.Items)
            _classifier.Enrich(item);

        _grouping.ApplySmartSort(fence);
        FenceStateService.Instance.MarkDirty();
    }
}
