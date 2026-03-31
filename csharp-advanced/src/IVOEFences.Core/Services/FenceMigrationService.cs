using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public static class FenceMigrationService
{
    public static void MigrateFenceItemsToEntities(IEnumerable<FenceModel> fences)
    {
        foreach (FenceModel fence in fences)
        {
            foreach (FenceItemModel item in fence.Items)
            {
                if (item.EntityId != Guid.Empty)
                    continue;

                DesktopEntity? entityProjection = DesktopEntityFactory.FromPath(item.TargetPath ?? string.Empty);
                if (entityProjection == null)
                {
                    item.IsUnresolved = true;
                    continue;
                }

                bool isDirectory = entityProjection.Kind == DesktopEntityKind.Directory;
                DesktopEntityModel model = DesktopEntityRegistryService.Instance.EnsureEntity(
                    entityProjection.FileSystemPath ?? entityProjection.ParsingPath,
                    entityProjection.DisplayName,
                    isDirectory);

                if (fence.Type == FenceType.Portal)
                    model.Ownership = DesktopItemOwnership.PortalManaged;
                else
                    DesktopEntityRegistryService.Instance.AssignToFence(model.Id, fence.Id);

                item.EntityId = model.Id;
                item.DisplayName = string.IsNullOrWhiteSpace(item.DisplayName) ? model.DisplayName : item.DisplayName;
                item.IsDirectory = isDirectory;
                item.TrackedFileType ??= model.Extension;
            }
        }
    }
}
