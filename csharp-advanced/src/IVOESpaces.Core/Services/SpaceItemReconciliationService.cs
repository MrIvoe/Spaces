using IVOESpaces.Core.Models;
using Serilog;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Reconciles space items after desktop watcher events or registry updates.
/// Removes duplicates for the same entity, syncs cached state from entity registry,
/// and marks items as unresolved if their entities are missing.
/// This is idempotent and safe to call frequently.
/// </summary>
public sealed class SpaceItemReconciliationService
{
    private static readonly Lazy<SpaceItemReconciliationService> _instance =
        new(() => new SpaceItemReconciliationService());

    public static SpaceItemReconciliationService Instance => _instance.Value;

    private SpaceItemReconciliationService()
    {
    }

    /// <summary>
    /// Reconciles all spaces in the system.
    /// </summary>
    public void ReconcileAllSpaces()
    {
        foreach (var space in SpaceStateService.Instance.Spaces)
            ReconcileSpace(space);

        SpaceStateService.Instance.MarkDirty();
        Log.Information("SpaceItemReconciliation: reconciled all spaces");
    }

    /// <summary>
    /// Reconciles a single space:
    /// 1. Removes duplicate entries for the same entity (keeping first occurrence)
    /// 2. Syncs cached state from entity registry for all items
    /// 3. Marks items as unresolved if their entities are missing
    /// </summary>
    public void ReconcileSpace(SpaceModel space)
    {
        if (space == null)
            return;

        int itemCountBefore = space.Items.Count;

        // Remove duplicates: keep only the first item referencing each entity
        var seen = new HashSet<Guid>();
        space.Items = space.Items
            .Where(item =>
            {
                Guid? entityId = item.EntityId != Guid.Empty ? item.EntityId : 
                                (item.DesktopEntityId != Guid.Empty ? item.DesktopEntityId : (Guid?)null);

                if (entityId.HasValue)
                {
                    if (seen.Contains(entityId.Value))
                    {
                        Log.Warning("SpaceItemReconciliation: removed duplicate item for entity {EntityId} in space {SpaceTitle}",
                            entityId.Value, space.Title);
                        return false;
                    }
                    seen.Add(entityId.Value);
                }

                return true;
            })
            .ToList();

        // Sync cached state from entity registry
        foreach (var item in space.Items)
        {
            var entity = SpaceItemResolver.Instance.ResolveEntity(item);
            if (entity != null)
            {
                SpaceItemResolver.Instance.SyncFromEntity(item, entity);
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

        int itemCountAfter = space.Items.Count;
        if (itemCountBefore != itemCountAfter)
        {
            Log.Information("SpaceItemReconciliation: space '{SpaceTitle}' items {Before} -> {After}",
                space.Title, itemCountBefore, itemCountAfter);
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
            
            ReconcileAllSpaces();
            Log.Information("SpaceItemReconciliation: created entity for {Path}", fullPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "SpaceItemReconciliation: error handling create for {Path}", fullPath);
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
            VirtualSpaceOwnershipService.Instance.RenamePath(oldPath, newPath, newDisplayName);
            ReconcileAllSpaces();
            Log.Information("SpaceItemReconciliation: renamed entity from {OldPath} to {NewPath}", oldPath, newPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "SpaceItemReconciliation: error handling rename from {OldPath} to {NewPath}", oldPath, newPath);
        }
    }

    /// <summary>
    /// Called by watcher after a delete event to mark entity missing and reconcile.
    /// </summary>
    public void OnDesktopItemDeleted(string fullPath)
    {
        try
        {
            VirtualSpaceOwnershipService.Instance.MarkMissingByPath(fullPath);
            ReconcileAllSpaces();
            Log.Information("SpaceItemReconciliation: marked missing entity for {Path}", fullPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "SpaceItemReconciliation: error handling delete for {Path}", fullPath);
        }
    }
}
