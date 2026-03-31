using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceDesktopSyncCoordinator
{
    public void ApplyDesktopChangeQueue(IEnumerable<FenceWindow> windows)
    {
        foreach (FenceWindow window in windows)
            window.InvalidateContent();

        FenceStateService.Instance.MarkDirty();
    }

    public FenceItemModel? CreateDesktopItemFromPath(string fullPath, string? displayName)
    {
        bool isDirectory = Directory.Exists(fullPath);
        bool isFile = File.Exists(fullPath);
        if (!isDirectory && !isFile)
            return null;

        DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
            fullPath,
            displayName,
            isDirectory);

        return new FenceItemModel
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
