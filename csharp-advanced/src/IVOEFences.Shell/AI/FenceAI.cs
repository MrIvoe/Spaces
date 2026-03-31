using IVOEFences.Core.Models;

namespace IVOEFences.Shell.AI;

internal sealed class FenceAI
{
    /// <summary>
    /// Suggest groups of frequently used, recently accessed icons.
    /// Baseline heuristic:
    /// - last access within 24h
    /// - usage count >= 3
    /// </summary>
    public List<List<FenceItemModel>> SuggestFences(List<FenceItemModel> allItems, int clusterCount = 3)
    {
        DateTime cutoff = DateTime.UtcNow.AddHours(-48);

        List<FenceItemModel> ranked = allItems
            .Where(i => i.OpenCount > 0 || (i.LastOpenedTime.HasValue && i.LastOpenedTime.Value >= cutoff))
            .OrderByDescending(GetUsageScore)
            .ThenByDescending(i => i.OpenCount)
            .ThenByDescending(i => i.LastOpenedTime ?? DateTime.MinValue)
            .ThenBy(i => i.DisplayName)
            .ToList();

        if (ranked.Count == 0)
            return new List<List<FenceItemModel>>();

        int boundedClusterCount = Math.Clamp(clusterCount, 1, Math.Min(6, ranked.Count));

        List<List<FenceItemModel>> clusters = Enumerable.Range(0, boundedClusterCount)
            .Select(_ => new List<FenceItemModel>())
            .ToList();

        int clusterIndex = 0;

        // Distribute by metadata bucket first, then round-robin inside each bucket
        // so heavy categories do not dominate a single fence.
        foreach (IGrouping<string, FenceItemModel> bucket in ranked
            .GroupBy(GetMetadataBucket)
            .OrderByDescending(g => g.Count()))
        {
            foreach (FenceItemModel item in bucket)
            {
                clusters[clusterIndex % boundedClusterCount].Add(item);
                clusterIndex++;
            }
        }

        return clusters.Where(c => c.Count > 0)
            .OrderByDescending(c => c.Count)
            .ToList();
    }

    private static double GetUsageScore(FenceItemModel item)
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

    private static string GetMetadataBucket(FenceItemModel item)
    {
        if (!string.IsNullOrWhiteSpace(item.TrackedFileType))
            return item.TrackedFileType!.ToLowerInvariant();

        if (item.IsDirectory)
            return "folder";

        return "other";
    }
}
