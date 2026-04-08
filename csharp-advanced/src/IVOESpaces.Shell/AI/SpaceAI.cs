using IVOESpaces.Core.Models;

namespace IVOESpaces.Shell.AI;

internal sealed class SpaceAI
{
    /// <summary>
    /// Suggest groups of frequently used, recently accessed icons.
    /// Baseline heuristic:
    /// - last access within 24h
    /// - usage count >= 3
    /// </summary>
    public List<List<SpaceItemModel>> SuggestSpaces(List<SpaceItemModel> allItems, int clusterCount = 3)
    {
        DateTime cutoff = DateTime.UtcNow.AddHours(-48);

        List<SpaceItemModel> ranked = allItems
            .Where(i => i.OpenCount > 0 || (i.LastOpenedTime.HasValue && i.LastOpenedTime.Value >= cutoff))
            .OrderByDescending(GetUsageScore)
            .ThenByDescending(i => i.OpenCount)
            .ThenByDescending(i => i.LastOpenedTime ?? DateTime.MinValue)
            .ThenBy(i => i.DisplayName)
            .ToList();

        if (ranked.Count == 0)
            return new List<List<SpaceItemModel>>();

        int boundedClusterCount = Math.Clamp(clusterCount, 1, Math.Min(6, ranked.Count));

        List<List<SpaceItemModel>> clusters = Enumerable.Range(0, boundedClusterCount)
            .Select(_ => new List<SpaceItemModel>())
            .ToList();

        int clusterIndex = 0;

        // Distribute by metadata bucket first, then round-robin inside each bucket
        // so heavy categories do not dominate a single space.
        foreach (IGrouping<string, SpaceItemModel> bucket in ranked
            .GroupBy(GetMetadataBucket)
            .OrderByDescending(g => g.Count()))
        {
            foreach (SpaceItemModel item in bucket)
            {
                clusters[clusterIndex % boundedClusterCount].Add(item);
                clusterIndex++;
            }
        }

        return clusters.Where(c => c.Count > 0)
            .OrderByDescending(c => c.Count)
            .ToList();
    }

    private static double GetUsageScore(SpaceItemModel item)
    {
        double openCountWeight = item.OpenCount * 4.0;
        double recencyWeight = 0.0;

        if (item.LastOpenedTime.HasValue)
        {
            double hoursSinceOpen = Math.Max((DateTime.UtcNow - item.LastOpenedTime.Value).TotalHours, 0);
            recencyWeight = 96.0 / (hoursSinceOpen + 2.0);
        }

        return openCountWeight + recencyWeight;
    }

    private static string GetMetadataBucket(SpaceItemModel item)
    {
        if (!string.IsNullOrWhiteSpace(item.TrackedFileType))
            return item.TrackedFileType!.ToLowerInvariant();

        if (item.IsDirectory)
            return "folder";

        return "other";
    }
}
