using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public static class SpaceImportService
{
    public static bool ImportPathIntoSpace(SpaceModel space, string path)
    {
        DesktopEntity? entityProjection = DesktopEntityFactory.FromPath(path);
        if (entityProjection == null)
            return false;

        DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
            entityProjection.FileSystemPath ?? entityProjection.ParsingPath,
            entityProjection.DisplayName,
            entityProjection.Kind == DesktopEntityKind.Directory);

        if (space.Type == SpaceType.Standard)
            VirtualSpaceOwnershipService.Instance.AssignToSpace(entity.Id, space.Id);

        if (space.Items.Any(i => i.EntityId == entity.Id))
            return false;

        space.Items.Add(new SpaceItemModel
        {
            EntityId = entity.Id,
            DisplayName = entity.DisplayName,
            TargetPath = entity.FileSystemPath ?? entity.ParsingName,
            IsDirectory = entity.IsDirectory,
            TrackedFileType = entity.Extension,
            SortOrder = space.Items.Count,
            IsFromDesktop = true,
        });

        SpaceStateService.Instance.MarkDirty();
        return true;
    }
}
