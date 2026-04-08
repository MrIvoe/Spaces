using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Resolves effective state for SpaceItemModel by preferring entity-backed values.
/// For standard spaces: EntityId points to DesktopEntity which is the primary source of truth.
/// Gracefully falls back to TargetPath when entity resolution fails (legacy data or transient state).
/// </summary>
public sealed class SpaceItemResolver
{
    private static readonly Lazy<SpaceItemResolver> _instance =
        new(() => new SpaceItemResolver());

    public static SpaceItemResolver Instance => _instance.Value;

    private SpaceItemResolver()
    {
    }

    /// <summary>
    /// Resolves the underlying DesktopEntity for a space item.
    /// Tries EntityId -> DesktopEntityId -> TargetPath lookup.
    /// Returns null if entity cannot be resolved.
    /// </summary>
    public DesktopEntity? ResolveEntity(SpaceItemModel item)
    {
        if (item.EntityId != Guid.Empty)
        {
            var entity = VirtualSpaceOwnershipService.Instance.GetEntity(item.EntityId);
            if (entity != null)
                return entity;
        }

        if (item.DesktopEntityId != Guid.Empty)
        {
            var entity = VirtualSpaceOwnershipService.Instance.GetEntity(item.DesktopEntityId);
            if (entity != null)
                return entity;
        }

        if (!string.IsNullOrWhiteSpace(item.TargetPath))
        {
            var model = DesktopEntityRegistryService.Instance.TryGetByPath(item.TargetPath);
            if (model != null)
                return ToDesktopEntity(model);
        }

        return null;
    }

    /// <summary>
    /// Returns the effective display name for a space item.
    /// Prefers custom label, then entity display name, then item display name.
    /// </summary>
    public string GetDisplayName(SpaceItemModel item)
    {
        if (!string.IsNullOrWhiteSpace(item.CustomLabel))
            return item.CustomLabel!;

        var entity = ResolveEntity(item);
        if (entity != null && !string.IsNullOrWhiteSpace(entity.DisplayName))
            return entity.DisplayName;

        return item.DisplayName ?? string.Empty;
    }

    /// <summary>
    /// Returns the effective filesystem path for a space item.
    /// Prefers entity path, falls back to TargetPath.
    /// </summary>
    public string GetPath(SpaceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null)
            return entity.FileSystemPath ?? entity.ParsingPath ?? string.Empty;

        return item.TargetPath ?? string.Empty;
    }

    /// <summary>
    /// Returns whether the item represents a directory.
    /// Prefers entity kind, falls back to IsDirectory flag.
    /// </summary>
    public bool IsDirectory(SpaceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null)
            return entity.Kind == DesktopEntityKind.Directory;

        return item.IsDirectory;
    }

    /// <summary>
    /// Returns whether the item is missing (path no longer exists or entity marked missing).
    /// </summary>
    public bool IsMissing(SpaceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null)
            return entity.IsMissing;

        return item.IsUnresolved;
    }

    /// <summary>
    /// Returns the effective file extension for a space item.
    /// Prefers entity extension, falls back to tracked file type.
    /// </summary>
    public string? GetExtension(SpaceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null && !string.IsNullOrWhiteSpace(entity.Extension))
            return entity.Extension;

        return item.TrackedFileType;
    }

    /// <summary>
    /// Syncs cached item state (DisplayName, TargetPath, IsDirectory, TrackedFileType) from entity.
    /// Used during reconciliation to keep space items fresh after watcher events.
    /// </summary>
    public void SyncFromEntity(SpaceItemModel item, DesktopEntity entity)
    {
        if (entity == null)
            return;

        item.EntityId = entity.Id;
        item.DesktopEntityId = entity.Id;
        item.DisplayName = entity.DisplayName;
        item.TargetPath = entity.FileSystemPath ?? entity.ParsingPath ?? string.Empty;
        item.IsDirectory = entity.Kind == DesktopEntityKind.Directory;
        item.IsUnresolved = entity.IsMissing;
        
        if (string.IsNullOrWhiteSpace(item.TrackedFileType) && !string.IsNullOrWhiteSpace(entity.Extension))
            item.TrackedFileType = entity.Extension;
    }

    private static DesktopEntity? ToDesktopEntity(DesktopEntityModel model)
    {
        if (model == null)
            return null;

        DesktopEntityKind kind = model.IsDirectory
            ? DesktopEntityKind.Directory
            : model.IsShortcut
                ? DesktopEntityKind.Shortcut
                : DesktopEntityKind.File;

        DesktopOwnershipMode mode = model.Ownership switch
        {
            DesktopItemOwnership.SpaceManaged => DesktopOwnershipMode.SpaceManaged,
            DesktopItemOwnership.PortalManaged => DesktopOwnershipMode.PortalManaged,
            DesktopItemOwnership.PinnedOverlay => DesktopOwnershipMode.WorkspaceProjected,
            _ => DesktopOwnershipMode.DesktopOnly,
        };

        return new DesktopEntity
        {
            Id = model.Id,
            DisplayName = model.DisplayName,
            ParsingPath = model.ParsingName,
            FileSystemPath = model.FileSystemPath,
            Kind = kind,
            Extension = model.Extension,
            OwnershipMode = mode,
            OwnerSpaceId = model.OwnerSpaceId,
            LastSeenUtc = model.LastSeenUtc,
            IsMissing = false,
        };
    }
}

