using IVOESpaces.Core.Models;
using Serilog;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Migrates legacy space items (with TargetPath but no EntityId) to the virtual ownership model.
/// Called once during SpaceStateService initialization.
/// </summary>
public static class SpaceEntityMigrationService
{
    public static void MigrateLoadedSpaces(IEnumerable<SpaceModel> spaces)
    {
        int migratedCount = 0;

        foreach (var space in spaces)
        {
            foreach (var item in space.Items)
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

                    // Set ownership mode based on space type
                    if (space.Type == SpaceType.Standard)
                    {
                        DesktopEntityRegistryService.Instance.AssignToSpace(entity.Id, space.Id);
                    }
                    else if (space.Type == SpaceType.Portal)
                    {
                        // Portal items are reference-based; don't assign ownership
                        DesktopEntityRegistryService.Instance.ReturnToDesktop(entity.Id);
                    }

                    // Link the space item to the entity
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
                    Log.Error(ex, "Failed to migrate space item '{DisplayName}' at '{TargetPath}'",
                        item.DisplayName, item.TargetPath);
                }
            }
        }

        if (migratedCount > 0)
            Log.Information("SpaceEntityMigrationService: migrated {Count} legacy space items to virtual ownership",
                migratedCount);
    }
}
