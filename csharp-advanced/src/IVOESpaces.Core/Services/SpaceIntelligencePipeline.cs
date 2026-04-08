using IVOESpaces.Core.Models;
using System.IO;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Unified precedence pipeline for space placement decisions.
/// Precedence: explicit sort rules -> behavior learning -> grouping suggester.
/// </summary>
public sealed class SpaceIntelligencePipeline
{
    private static readonly Lazy<SpaceIntelligencePipeline> _instance = new(() => new SpaceIntelligencePipeline());
    public static SpaceIntelligencePipeline Instance => _instance.Value;

    private SpaceIntelligencePipeline()
    {
    }

    public Guid? ResolveTargetSpace(SpaceItemModel item, IReadOnlyList<SpaceModel> spaces)
    {
        Guid? ruleSpace = SortRulesEngine.Instance.DetermineSpaceForItem(item);
        if (ruleSpace.HasValue)
            return ruleSpace;

        string? ext = string.IsNullOrWhiteSpace(item.TrackedFileType)
            ? Path.GetExtension(item.TargetPath)
            : item.TrackedFileType;

        if (!string.IsNullOrWhiteSpace(ext))
        {
            Guid? learnedSpace = BehaviorLearningService.Instance.TrySuggestSpaceForExtension(ext);
            if (learnedSpace.HasValue)
                return learnedSpace;
        }

        var suggestions = SpaceIconGroupingSuggester.Instance.GetSpaceSuggestions(item, spaces, 1);
        if (suggestions.Count > 0)
            return suggestions[0].Space.Id;

        return null;
    }
}
