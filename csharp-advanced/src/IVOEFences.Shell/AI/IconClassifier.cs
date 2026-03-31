using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.AI;

internal sealed class IconClassifier
{
    public void Enrich(FenceItemModel item)
    {
        FenceUsageTracker.Instance.CacheFileMetadata(item);
        IconMetadataService.Instance.AutoTag(item);
    }

    public string Classify(FenceItemModel item)
    {
        return FenceIconGroupingSuggester.InferFenceCategory(new FenceModel
        {
            Items = new List<FenceItemModel> { item }
        }) ?? "Uncategorized";
    }
}
