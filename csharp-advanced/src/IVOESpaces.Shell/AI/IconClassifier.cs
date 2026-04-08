using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.AI;

internal sealed class IconClassifier
{
    public void Enrich(SpaceItemModel item)
    {
        SpaceUsageTracker.Instance.CacheFileMetadata(item);
        IconMetadataService.Instance.AutoTag(item);
    }

    public string Classify(SpaceItemModel item)
    {
        return SpaceIconGroupingSuggester.InferSpaceCategory(new SpaceModel
        {
            Items = new List<SpaceItemModel> { item }
        }) ?? "Uncategorized";
    }
}
