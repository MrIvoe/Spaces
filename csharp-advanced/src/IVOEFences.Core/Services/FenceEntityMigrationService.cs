using IVOEFences.Core.Models;
using Serilog;

namespace IVOEFences.Core.Services;

/// <summary>
/// Migrates legacy fence items (with TargetPath but no EntityId) to the virtual ownership model.
/// Called once during FenceStateService initialization.
/// </summary>
public static class FenceEntityMigrationService
{
    public static void MigrateLoadedFences(IEnumerable<FenceModel> fences)
    {
        int migratedCount = 0;

        foreach (var fence in fences)
        {
            foreach (var item in fence.Items)
            {
                // Skip already-migrated items
                if (item.EntityId != Guid.Empty)
                    continue;

                // Skip items with no path reference
                if (string.IsNullOrWhiteSpace(item.TargetPath))
                    continue;

                try
                {
                    // Ensure entity exists in the registry
                    DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
                        item.TargetPath,
                        string.IsNullOrWhiteSpace(item.DisplayName) ? null : item.DisplayName,
                        item.IsDirectory);

                    // Set ownership mode based on fence type
                    if (fence.Type == FenceType.Standard)
                    {
                        DesktopEntityRegistryService.Instance.AssignToFence(entity.Id, fence.Id);
                    }
                    else if (fence.Type == FenceType.Portal)
                    {
                        // Portal items are reference-based; don't assign ownership
                        DesktopEntityRegistryService.Instance.ReturnToDesktop(entity.Id);
                    }

                    // Link the fence item to the entity
                    item.EntityId = entity.Id;

                    // Sync display name if not already set
                    if (string.IsNullOrWhiteSpace(item.DisplayName))
                        item.DisplayName = entity.DisplayName;

                    // Sync directory flag
                    if (item.TargetPath != null && !item.IsDirectory)
                        item.IsDirectory = entity.IsDirectory;

                    // Cache file type if not already set
                    if (string.IsNullOrWhiteSpace(item.TrackedFileType) && entity.Extension != null)
                        item.TrackedFileType = entity.Extension;

                    migratedCount++;
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to migrate fence item '{DisplayName}' at '{TargetPath}'",
                        item.DisplayName, item.TargetPath);
                }
            }
        }

        if (migratedCount > 0)
            Log.Information("FenceEntityMigrationService: migrated {Count} legacy fence items to virtual ownership",
                migratedCount);
    }
}
