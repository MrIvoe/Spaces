using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceDesktopSyncCoordinator
{
    public void ApplyDesktopChangeQueue(IEnumerable<SpaceWindow> windows)
    {
        foreach (SpaceWindow window in windows)
            window.InvalidateContent();

        SpaceStateService.Instance.MarkDirty();
    }

    public SpaceItemModel? CreateDesktopItemFromPath(string fullPath, string? displayName)
    {
        bool isDirectory = Directory.Exists(fullPath);
        bool isFile = File.Exists(fullPath);
        if (!isDirectory && !isFile)
            return null;

        DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
            fullPath,
            displayName,
            isDirectory);

        return new SpaceItemModel
        {
            Id = Guid.NewGuid(),
            DesktopEntityId = entity.Id,
            TargetPath = fullPath,
            DisplayName = string.IsNullOrWhiteSpace(displayName)
                ? (isDirectory ? Path.GetFileName(fullPath) : Path.GetFileNameWithoutExtension(fullPath))
                : displayName,
            IsDirectory = isDirectory,
            IsFromDesktop = true,
            TrackedFileType = isDirectory ? "folder" : Path.GetExtension(fullPath),
        };
    }
}
