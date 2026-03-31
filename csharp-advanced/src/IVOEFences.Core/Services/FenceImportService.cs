using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public static class FenceImportService
{
    public static bool ImportPathIntoFence(FenceModel fence, string path)
    {
        DesktopEntity? entityProjection = DesktopEntityFactory.FromPath(path);
        if (entityProjection == null)
            return false;

        DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
            entityProjection.FileSystemPath ?? entityProjection.ParsingPath,
            entityProjection.DisplayName,
            entityProjection.Kind == DesktopEntityKind.Directory);

        if (fence.Type == FenceType.Standard)
            VirtualFenceOwnershipService.Instance.AssignToFence(entity.Id, fence.Id);

        if (fence.Items.Any(i => i.EntityId == entity.Id))
            return false;

        fence.Items.Add(new FenceItemModel
        {
            EntityId = entity.Id,
            DisplayName = entity.DisplayName,
            TargetPath = entity.FileSystemPath ?? entity.ParsingName,
            IsDirectory = entity.IsDirectory,
            TrackedFileType = entity.Extension,
            SortOrder = fence.Items.Count,
            IsFromDesktop = true,
        });

        FenceStateService.Instance.MarkDirty();
        return true;
    }
}
