using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Resolves effective state for FenceItemModel by preferring entity-backed values.
/// For standard fences: EntityId points to DesktopEntity which is the primary source of truth.
/// Gracefully falls back to TargetPath when entity resolution fails (legacy data or transient state).
/// </summary>
public sealed class FenceItemResolver
{
    private static readonly Lazy<FenceItemResolver> _instance =
        new(() => new FenceItemResolver());

    public static FenceItemResolver Instance => _instance.Value;

    private FenceItemResolver()
    {
    }

    /// <summary>
    /// Resolves the underlying DesktopEntity for a fence item.
    /// Tries EntityId -> DesktopEntityId -> TargetPath lookup.
    /// Returns null if entity cannot be resolved.
    /// </summary>
    public DesktopEntity? ResolveEntity(FenceItemModel item)
    {
        if (item.EntityId != Guid.Empty)
        {
            var entity = VirtualFenceOwnershipService.Instance.GetEntity(item.EntityId);
            if (entity != null)
                return entity;
        }

        if (item.DesktopEntityId != Guid.Empty)
        {
            var entity = VirtualFenceOwnershipService.Instance.GetEntity(item.DesktopEntityId);
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
    /// Returns the effective display name for a fence item.
    /// Prefers custom label, then entity display name, then item display name.
    /// </summary>
    public string GetDisplayName(FenceItemModel item)
    {
        if (!string.IsNullOrWhiteSpace(item.CustomLabel))
            return item.CustomLabel!;

        var entity = ResolveEntity(item);
        if (entity != null && !string.IsNullOrWhiteSpace(entity.DisplayName))
            return entity.DisplayName;

        return item.DisplayName ?? string.Empty;
    }

    /// <summary>
    /// Returns the effective filesystem path for a fence item.
    /// Prefers entity path, falls back to TargetPath.
    /// </summary>
    public string GetPath(FenceItemModel item)
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
    public bool IsDirectory(FenceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null)
            return entity.Kind == DesktopEntityKind.Directory;

        return item.IsDirectory;
    }

    /// <summary>
    /// Returns whether the item is missing (path no longer exists or entity marked missing).
    /// </summary>
    public bool IsMissing(FenceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null)
            return entity.IsMissing;

        return item.IsUnresolved;
    }

    /// <summary>
    /// Returns the effective file extension for a fence item.
    /// Prefers entity extension, falls back to tracked file type.
    /// </summary>
    public string? GetExtension(FenceItemModel item)
    {
        var entity = ResolveEntity(item);
        if (entity != null && !string.IsNullOrWhiteSpace(entity.Extension))
            return entity.Extension;

        return item.TrackedFileType;
    }

    /// <summary>
    /// Syncs cached item state (DisplayName, TargetPath, IsDirectory, TrackedFileType) from entity.
    /// Used during reconciliation to keep fence items fresh after watcher events.
    /// </summary>
    public void SyncFromEntity(FenceItemModel item, DesktopEntity entity)
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
            DesktopItemOwnership.FenceManaged => DesktopOwnershipMode.FenceManaged,
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
            OwnerFenceId = model.OwnerFenceId,
            LastSeenUtc = model.LastSeenUtc,
            IsMissing = false,
        };
    }
}

