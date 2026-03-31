using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Compatibility facade exposing virtual ownership semantics while delegating
/// durable state persistence to DesktopEntityRegistryService.
/// </summary>
public sealed class VirtualFenceOwnershipService
{
    private static readonly Lazy<VirtualFenceOwnershipService> _instance = new(() => new VirtualFenceOwnershipService());
    public static VirtualFenceOwnershipService Instance => _instance.Value;

    private VirtualFenceOwnershipService()
    {
    }

    public IReadOnlyCollection<DesktopEntity> GetAllEntities()
    {
        // Registry currently supports point lookups, so gather from known fences/items.
        var entities = new List<DesktopEntity>();
        var seen = new HashSet<Guid>();

        foreach (FenceModel fence in FenceStateService.Instance.Fences)
        {
            foreach (FenceItemModel item in fence.Items)
            {
                Guid id = item.EntityId;
                if (id == Guid.Empty || !seen.Add(id))
                    continue;

                DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(id);
                if (model != null)
                    entities.Add(ToDesktopEntity(model));
            }
        }

        return entities.AsReadOnly();
    }

    public DesktopEntity? GetEntity(Guid id)
    {
        DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(id);
        return model == null ? null : ToDesktopEntity(model);
    }

    public DesktopEntity UpsertEntity(DesktopEntity entity)
    {
        DesktopEntityModel model = DesktopEntityRegistryService.Instance.EnsureEntity(
            entity.FileSystemPath ?? entity.ParsingPath,
            entity.DisplayName,
            entity.Kind == DesktopEntityKind.Directory);

        if (entity.OwnerFenceId.HasValue && entity.OwnershipMode == DesktopOwnershipMode.FenceManaged)
            DesktopEntityRegistryService.Instance.AssignToFence(model.Id, entity.OwnerFenceId.Value);
        else if (!entity.OwnerFenceId.HasValue && entity.OwnershipMode == DesktopOwnershipMode.DesktopOnly)
            DesktopEntityRegistryService.Instance.ReturnToDesktop(model.Id);

        return ToDesktopEntity(DesktopEntityRegistryService.Instance.TryGetById(model.Id)!);
    }

    public void AssignToFence(Guid entityId, Guid fenceId) => DesktopEntityRegistryService.Instance.AssignToFence(entityId, fenceId);

    public void ReleaseToDesktop(Guid entityId) => DesktopEntityRegistryService.Instance.ReturnToDesktop(entityId);

    public bool ShouldRenderOnDesktop(Guid entityId)
    {
        DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(entityId);
        return model != null && model.Ownership == DesktopItemOwnership.DesktopOnly;
    }

    public IReadOnlyList<DesktopEntity> GetFenceEntities(Guid fenceId)
    {
        var result = new List<DesktopEntity>();
        foreach (FenceModel fence in FenceStateService.Instance.Fences.Where(f => f.Id == fenceId))
        {
            foreach (FenceItemModel item in fence.Items)
            {
                if (item.EntityId == Guid.Empty)
                    continue;

                DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(item.EntityId);
                if (model == null || model.OwnerFenceId != fenceId || model.Ownership != DesktopItemOwnership.FenceManaged)
                    continue;

                result.Add(ToDesktopEntity(model));
            }
        }

        return result;
    }

    public void MarkMissingByPath(string path)
    {
        DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetByPath(path);
        if (model == null)
            return;

        model.LastSeenUtc = DateTime.UtcNow;
    }

    public void RenamePath(string oldPath, string newPath, string newDisplayName)
    {
        DesktopEntityRegistryService.Instance.HandleRename(oldPath, newPath, newDisplayName);
    }

    private static DesktopEntity ToDesktopEntity(DesktopEntityModel model)
    {
        DesktopOwnershipMode mode = model.Ownership switch
        {
            DesktopItemOwnership.FenceManaged => DesktopOwnershipMode.FenceManaged,
            DesktopItemOwnership.PortalManaged => DesktopOwnershipMode.PortalManaged,
            DesktopItemOwnership.PinnedOverlay => DesktopOwnershipMode.WorkspaceProjected,
            _ => DesktopOwnershipMode.DesktopOnly,
        };

        DesktopEntityKind kind = model.IsDirectory
            ? DesktopEntityKind.Directory
            : model.IsShortcut
                ? DesktopEntityKind.Shortcut
                : DesktopEntityKind.File;

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
