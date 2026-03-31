using IVOEFences.Core.Models;
using System.IO;

namespace IVOEFences.Core.Services;

/// <summary>
/// Unified precedence pipeline for fence placement decisions.
/// Precedence: explicit sort rules -> behavior learning -> grouping suggester.
/// </summary>
public sealed class FenceIntelligencePipeline
{
    private static readonly Lazy<FenceIntelligencePipeline> _instance = new(() => new FenceIntelligencePipeline());
    public static FenceIntelligencePipeline Instance => _instance.Value;

    private FenceIntelligencePipeline()
    {
    }

    public Guid? ResolveTargetFence(FenceItemModel item, IReadOnlyList<FenceModel> fences)
    {
        Guid? ruleFence = SortRulesEngine.Instance.DetermineFenceForItem(item);
        if (ruleFence.HasValue)
            return ruleFence;

        string? ext = string.IsNullOrWhiteSpace(item.TrackedFileType)
            ? Path.GetExtension(item.TargetPath)
            : item.TrackedFileType;

        if (!string.IsNullOrWhiteSpace(ext))
        {
            Guid? learnedFence = BehaviorLearningService.Instance.TrySuggestFenceForExtension(ext);
            if (learnedFence.HasValue)
                return learnedFence;
        }

        var suggestions = FenceIconGroupingSuggester.Instance.GetFenceSuggestions(item, fences, 1);
        if (suggestions.Count > 0)
            return suggestions[0].Fence.Id;

        return null;
    }
}
