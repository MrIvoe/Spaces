using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public static class SpaceMigrationService
{
    public static void MigrateSpaceItemsToEntities(IEnumerable<SpaceModel> spaces)
    {
        foreach (SpaceModel space in spaces)
        {
            foreach (SpaceItemModel item in space.Items)
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

                if (space.Type == SpaceType.Portal)
                    model.Ownership = DesktopItemOwnership.PortalManaged;
                else
                    DesktopEntityRegistryService.Instance.AssignToSpace(model.Id, space.Id);

                item.EntityId = model.Id;
                item.DisplayName = string.IsNullOrWhiteSpace(item.DisplayName) ? model.DisplayName : item.DisplayName;
                item.IsDirectory = isDirectory;
                item.TrackedFileType ??= model.Extension;
            }
        }
    }
}
