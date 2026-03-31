using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Resolves duplicate path ownership across fence models so a desktop item has
/// one deterministic visible owner (portal or standard fence), avoiding
/// duplicate visual presence across runtime surfaces.
/// </summary>
public sealed class DesktopOwnershipReconciliationService
{
    private static readonly Lazy<DesktopOwnershipReconciliationService> _instance = new(() => new DesktopOwnershipReconciliationService());
    public static DesktopOwnershipReconciliationService Instance => _instance.Value;

    private DesktopOwnershipReconciliationService()
    {
    }

    public ReconciliationResult Reconcile(IReadOnlyList<FenceModel> fences)
    {
        if (fences.Count == 0)
            return new ReconciliationResult(0, 0);

        var seenByPath = new Dictionary<string, (FenceModel Fence, FenceItemModel Item, int Priority)>(StringComparer.OrdinalIgnoreCase);
        int removedDuplicates = 0;

        foreach (FenceModel fence in fences.OrderBy(GetFencePriority).ThenBy(f => f.Id))
        {
            for (int i = fence.Items.Count - 1; i >= 0; i--)
            {
                FenceItemModel item = fence.Items[i];
                string key = NormalizePathKey(item.TargetPath);
                if (string.IsNullOrEmpty(key))
                    continue;

                int priority = GetItemPriority(fence);
                if (!seenByPath.TryGetValue(key, out var winner))
                {
                    seenByPath[key] = (fence, item, priority);
                    continue;
                }

                if (priority < winner.Priority)
                {
                    winner.Fence.Items.Remove(winner.Item);
                    seenByPath[key] = (fence, item, priority);
                    removedDuplicates++;
                    continue;
                }

                fence.Items.RemoveAt(i);
                removedDuplicates++;
            }
        }

        int normalizedSortCount = 0;
        foreach (FenceModel fence in fences)
        {
            for (int i = 0; i < fence.Items.Count; i++)
            {
                if (fence.Items[i].SortOrder == i)
                    continue;

                fence.Items[i].SortOrder = i;
                normalizedSortCount++;
            }
        }

        return new ReconciliationResult(removedDuplicates, normalizedSortCount);
    }

    private static string NormalizePathKey(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return string.Empty;

        try
        {
            return Path.GetFullPath(path).TrimEnd('\\');
        }
        catch
        {
            return path.Trim().TrimEnd('\\');
        }
    }

    private static int GetFencePriority(FenceModel fence)
    {
        return fence.Type switch
        {
            FenceType.Portal => 0,
            FenceType.Standard => 1,
            _ => 2,
        };
    }

    private static int GetItemPriority(FenceModel fence)
    {
        return fence.Type switch
        {
            FenceType.Portal => 0,
            FenceType.Standard => 1,
            _ => 2,
        };
    }

    public readonly record struct ReconciliationResult(int RemovedDuplicateItems, int NormalizedSortOrders);
}
