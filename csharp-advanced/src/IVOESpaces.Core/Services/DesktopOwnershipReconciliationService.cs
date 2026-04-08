using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Resolves duplicate path ownership across space models so a desktop item has
/// one deterministic visible owner (portal or standard space), avoiding
/// duplicate visual presence across runtime surfaces.
/// </summary>
public sealed class DesktopOwnershipReconciliationService
{
    private static readonly Lazy<DesktopOwnershipReconciliationService> _instance = new(() => new DesktopOwnershipReconciliationService());
    public static DesktopOwnershipReconciliationService Instance => _instance.Value;

    private DesktopOwnershipReconciliationService()
    {
    }

    public ReconciliationResult Reconcile(IReadOnlyList<SpaceModel> spaces)
    {
        if (spaces.Count == 0)
            return new ReconciliationResult(0, 0);

        var seenByPath = new Dictionary<string, (SpaceModel Space, SpaceItemModel Item, int Priority)>(StringComparer.OrdinalIgnoreCase);
        int removedDuplicates = 0;

        foreach (SpaceModel space in spaces.OrderBy(GetSpacePriority).ThenBy(f => f.Id))
        {
            for (int i = space.Items.Count - 1; i >= 0; i--)
            {
                SpaceItemModel item = space.Items[i];
                string key = NormalizePathKey(item.TargetPath);
                if (string.IsNullOrEmpty(key))
                    continue;

                int priority = GetItemPriority(space);
                if (!seenByPath.TryGetValue(key, out var winner))
                {
                    seenByPath[key] = (space, item, priority);
                    continue;
                }

                if (priority < winner.Priority)
                {
                    winner.Space.Items.Remove(winner.Item);
                    seenByPath[key] = (space, item, priority);
                    removedDuplicates++;
                    continue;
                }

                space.Items.RemoveAt(i);
                removedDuplicates++;
            }
        }

        int normalizedSortCount = 0;
        foreach (SpaceModel space in spaces)
        {
            for (int i = 0; i < space.Items.Count; i++)
            {
                if (space.Items[i].SortOrder == i)
                    continue;

                space.Items[i].SortOrder = i;
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

    private static int GetSpacePriority(SpaceModel space)
    {
        return space.Type switch
        {
            SpaceType.Portal => 0,
            SpaceType.Standard => 1,
            _ => 2,
        };
    }

    private static int GetItemPriority(SpaceModel space)
    {
        return space.Type switch
        {
            SpaceType.Portal => 0,
            SpaceType.Standard => 1,
            _ => 2,
        };
    }

    public readonly record struct ReconciliationResult(int RemovedDuplicateItems, int NormalizedSortOrders);
}
