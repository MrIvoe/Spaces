using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell.AI;

internal sealed class AIManager
{
    private readonly IconClassifier _classifier = new();
    private readonly GroupingEngine _grouping = new();

    public IReadOnlyList<SpaceIconGroupingSuggester.GroupingSuggestion> AnalyzeAndSuggest(SpaceItemModel item)
    {
        _classifier.Enrich(item);
        var suggestions = _grouping.Suggest(item);

        if (suggestions.Count > 0)
        {
            item.LastSuggestedMoveTime = DateTime.UtcNow;
            Log.Information("AIManager: top suggestion for {Item} is {Space} ({Score:0.00})",
                item.DisplayName,
                suggestions[0].Space.Title,
                suggestions[0].ConfidenceScore);
        }

        return suggestions;
    }

    public void SmartGroupSpace(SpaceModel space)
    {
        foreach (SpaceItemModel item in space.Items)
            _classifier.Enrich(item);

        _grouping.ApplySmartSort(space);
        SpaceStateService.Instance.MarkDirty();
    }
}
