using IVOEFences.Core.Models;
using Serilog;

namespace IVOEFences.Core.Services;

/// <summary>
/// Reconciles fence items after desktop watcher events or registry updates.
/// Removes duplicates for the same entity, syncs cached state from entity registry,
/// and marks items as unresolved if their entities are missing.
/// This is idempotent and safe to call frequently.
/// </summary>
public sealed class FenceItemReconciliationService
{
    private static readonly Lazy<FenceItemReconciliationService> _instance =
        new(() => new FenceItemReconciliationService());

    public static FenceItemReconciliationService Instance => _instance.Value;

    private FenceItemReconciliationService()
    {
    }

    /// <summary>
    /// Reconciles all fences in the system.
    /// </summary>
    public void ReconcileAllFences()
    {
        foreach (var fence in FenceStateService.Instance.Fences)
            ReconcileFence(fence);

        FenceStateService.Instance.MarkDirty();
        Log.Information("FenceItemReconciliation: reconciled all fences");
    }

    /// <summary>
    /// Reconciles a single fence:
    /// 1. Removes duplicate entries for the same entity (keeping first occurrence)
    /// 2. Syncs cached state from entity registry for all items
    /// 3. Marks items as unresolved if their entities are missing
    /// </summary>
    public void ReconcileFence(FenceModel fence)
    {
        if (fence == null)
            return;

        int itemCountBefore = fence.Items.Count;

        // Remove duplicates: keep only the first item referencing each entity
        var seen = new HashSet<Guid>();
        fence.Items = fence.Items
            .Where(item =>
            {
                Guid? entityId = item.EntityId != Guid.Empty ? item.EntityId : 
                                (item.DesktopEntityId != Guid.Empty ? item.DesktopEntityId : (Guid?)null);

                if (entityId.HasValue)
                {
                    if (seen.Contains(entityId.Value))
                    {
                        Log.Warning("FenceItemReconciliation: removed duplicate item for entity {EntityId} in fence {FenceTitle}",
                            entityId.Value, fence.Title);
                        return false;
                    }
                    seen.Add(entityId.Value);
                }

                return true;
            })
            .ToList();

        // Sync cached state from entity registry
        foreach (var item in fence.Items)
        {
            var entity = FenceItemResolver.Instance.ResolveEntity(item);
            if (entity != null)
            {
                FenceItemResolver.Instance.SyncFromEntity(item, entity);
            }
            else
            {
                // Entity not found: check if file exists at cached path, otherwise mark unresolved
                string path = item.TargetPath ?? string.Empty;
                if (!string.IsNullOrWhiteSpace(path))
                {
                    if (!File.Exists(path) && !Directory.Exists(path))
                        item.IsUnresolved = true;
                }
            }
        }

        int itemCountAfter = fence.Items.Count;
        if (itemCountBefore != itemCountAfter)
        {
            Log.Information("FenceItemReconciliation: fence '{FenceTitle}' items {Before} -> {After}",
                fence.Title, itemCountBefore, itemCountAfter);
        }
    }

    /// <summary>
    /// Called by watcher after a create event to ensure entity exists and reconcile.
    /// </summary>
    public void OnDesktopItemCreated(string fullPath)
    {
        try
        {
            var entity = DesktopEntityRegistryService.Instance.EnsureEntity(
                fullPath,
                Path.GetFileNameWithoutExtension(fullPath),
                Directory.Exists(fullPath));
            
            ReconcileAllFences();
            Log.Information("FenceItemReconciliation: created entity for {Path}", fullPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "FenceItemReconciliation: error handling create for {Path}", fullPath);
        }
    }

    /// <summary>
    /// Called by watcher after a rename event to update entity path and reconcile.
    /// </summary>
    public void OnDesktopItemRenamed(string oldPath, string newPath)
    {
        try
        {
            string newDisplayName = Path.GetFileNameWithoutExtension(newPath);
            VirtualFenceOwnershipService.Instance.RenamePath(oldPath, newPath, newDisplayName);
            ReconcileAllFences();
            Log.Information("FenceItemReconciliation: renamed entity from {OldPath} to {NewPath}", oldPath, newPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "FenceItemReconciliation: error handling rename from {OldPath} to {NewPath}", oldPath, newPath);
        }
    }

    /// <summary>
    /// Called by watcher after a delete event to mark entity missing and reconcile.
    /// </summary>
    public void OnDesktopItemDeleted(string fullPath)
    {
        try
        {
            VirtualFenceOwnershipService.Instance.MarkMissingByPath(fullPath);
            ReconcileAllFences();
            Log.Information("FenceItemReconciliation: marked missing entity for {Path}", fullPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "FenceItemReconciliation: error handling delete for {Path}", fullPath);
        }
    }
}
