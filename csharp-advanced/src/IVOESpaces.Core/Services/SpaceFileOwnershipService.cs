using IVOESpaces.Core.Models;
using System.IO;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Manages virtual ownership for standard spaces.
/// Files stay in place; space membership is tracked as ownership state over durable entities.
/// </summary>
public sealed class SpaceFileOwnershipService
{
    private static readonly Lazy<SpaceFileOwnershipService> _instance = new(() => new SpaceFileOwnershipService());
    public static SpaceFileOwnershipService Instance => _instance.Value;

    private readonly DesktopEntityRegistryService _registry;
    private readonly string _desktopDir;
    private readonly string _commonDesktopDir;

    private SpaceFileOwnershipService()
        : this(
            DesktopEntityRegistryService.Instance,
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory))
    {
    }

    internal SpaceFileOwnershipService(
        DesktopEntityRegistryService registry,
        string desktopDir,
        string commonDesktopDir)
    {
        _registry = registry;
        _desktopDir = Normalize(desktopDir);
        _commonDesktopDir = Normalize(commonDesktopDir);
    }

    public static SpaceFileOwnershipService CreateForTesting(
        DesktopEntityRegistryService registry,
        string desktopDir,
        string commonDesktopDir)
    {
        return new SpaceFileOwnershipService(registry, desktopDir, commonDesktopDir);
    }

    public string SpaceRootDirectory => Path.Combine(_desktopDir, ".spaces");

    public bool EnsureSpaceItemOwnership(SpaceModel space, SpaceItemModel item)
    {
        if (space.Type != SpaceType.Standard)
            return false;

        if (string.IsNullOrWhiteSpace(item.TargetPath))
            return false;

        string sourcePath = item.TargetPath;
        bool isDirectory = Directory.Exists(sourcePath);
        bool isFile = File.Exists(sourcePath);
        if (!isDirectory && !isFile)
            return false;

        DesktopEntityModel entity = item.DesktopEntityId != Guid.Empty
            ? _registry.TryGetById(item.DesktopEntityId)
                ?? _registry.EnsureEntity(sourcePath, item.DisplayName, isDirectory)
            : _registry.EnsureEntity(sourcePath, item.DisplayName, isDirectory);

        item.DesktopEntityId = entity.Id;
        item.TargetPath = entity.FileSystemPath ?? sourcePath;
        item.IsFromDesktop = false;
        item.DisplayName = string.IsNullOrWhiteSpace(item.DisplayName) ? entity.DisplayName : item.DisplayName;

        _registry.AssignToSpace(entity.Id, space.Id);
        Serilog.Log.Debug(
            "Space ownership: assigned entity {EntityId} path '{Path}' to space '{Space}' ({SpaceId})",
            entity.Id,
            item.TargetPath,
            space.Title,
            space.Id);
        return true;
    }

    public bool ReleaseSpaceItemToDesktop(SpaceItemModel item)
    {
        if (string.IsNullOrWhiteSpace(item.TargetPath))
            return false;

        string sourcePath = item.TargetPath;
        if (!File.Exists(sourcePath) && !Directory.Exists(sourcePath))
            return false;

        if (item.DesktopEntityId == Guid.Empty)
        {
            bool isDirectory = Directory.Exists(sourcePath);
            DesktopEntityModel created = _registry.EnsureEntity(sourcePath, item.DisplayName, isDirectory);
            item.DesktopEntityId = created.Id;
        }

        _registry.ReturnToDesktop(item.DesktopEntityId);
        item.IsFromDesktop = true;
        item.DisplayName = item.IsDirectory ? Path.GetFileName(sourcePath) : Path.GetFileNameWithoutExtension(sourcePath);

        Serilog.Log.Debug(
            "Space ownership: released entity {EntityId} path '{Source}' to desktop",
            item.DesktopEntityId,
            sourcePath);
        return true;
    }

    public bool IsDesktopManagedPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return false;

        string fullPath;
        try
        {
            fullPath = Normalize(Path.GetFullPath(path));
        }
        catch
        {
            return false;
        }

        return IsUnderRoot(fullPath, _desktopDir)
            || IsUnderRoot(fullPath, _commonDesktopDir);
    }

    // Legacy directory name retained for compatibility with old state and tooling.
    // Virtual ownership no longer moves items into this folder.

    private static bool IsUnderRoot(string fullPath, string root)
    {
        if (string.IsNullOrWhiteSpace(root))
            return false;

        return fullPath.StartsWith(root + "\\", StringComparison.OrdinalIgnoreCase)
            || string.Equals(fullPath, root, StringComparison.OrdinalIgnoreCase);
    }

    private static string Normalize(string path) => path.TrimEnd('\\');
}
