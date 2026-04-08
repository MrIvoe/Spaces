using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Compatibility facade exposing virtual ownership semantics while delegating
/// durable state persistence to DesktopEntityRegistryService.
/// </summary>
public sealed class VirtualSpaceOwnershipService
{
    private static readonly Lazy<VirtualSpaceOwnershipService> _instance = new(() => new VirtualSpaceOwnershipService());
    public static VirtualSpaceOwnershipService Instance => _instance.Value;

    private VirtualSpaceOwnershipService()
    {
    }

    public IReadOnlyCollection<DesktopEntity> GetAllEntities()
    {
        // Registry currently supports point lookups, so gather from known spaces/items.
        var entities = new List<DesktopEntity>();
        var seen = new HashSet<Guid>();

        foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
        {
            foreach (SpaceItemModel item in space.Items)
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

        if (entity.OwnerSpaceId.HasValue && entity.OwnershipMode == DesktopOwnershipMode.SpaceManaged)
            DesktopEntityRegistryService.Instance.AssignToSpace(model.Id, entity.OwnerSpaceId.Value);
        else if (!entity.OwnerSpaceId.HasValue && entity.OwnershipMode == DesktopOwnershipMode.DesktopOnly)
            DesktopEntityRegistryService.Instance.ReturnToDesktop(model.Id);

        return ToDesktopEntity(DesktopEntityRegistryService.Instance.TryGetById(model.Id)!);
    }

    public void AssignToSpace(Guid entityId, Guid spaceId) => DesktopEntityRegistryService.Instance.AssignToSpace(entityId, spaceId);

    public void ReleaseToDesktop(Guid entityId) => DesktopEntityRegistryService.Instance.ReturnToDesktop(entityId);

    public bool ShouldRenderOnDesktop(Guid entityId)
    {
        DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(entityId);
        return model != null && model.Ownership == DesktopItemOwnership.DesktopOnly;
    }

    public IReadOnlyList<DesktopEntity> GetSpaceEntities(Guid spaceId)
    {
        var result = new List<DesktopEntity>();
        foreach (SpaceModel space in SpaceStateService.Instance.Spaces.Where(f => f.Id == spaceId))
        {
            foreach (SpaceItemModel item in space.Items)
            {
                if (item.EntityId == Guid.Empty)
                    continue;

                DesktopEntityModel? model = DesktopEntityRegistryService.Instance.TryGetById(item.EntityId);
                if (model == null || model.OwnerSpaceId != spaceId || model.Ownership != DesktopItemOwnership.SpaceManaged)
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
            DesktopItemOwnership.SpaceManaged => DesktopOwnershipMode.SpaceManaged,
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
            OwnerSpaceId = model.OwnerSpaceId,
            LastSeenUtc = model.LastSeenUtc,
            IsMissing = false,
        };
    }
}
