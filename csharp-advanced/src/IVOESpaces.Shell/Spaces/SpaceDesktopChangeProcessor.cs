using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceDesktopChangeProcessor
{
    internal enum DesktopChangeKind
    {
        Created,
        Deleted,
        Renamed,
    }

    internal sealed record DesktopChange(
        DesktopChangeKind Kind,
        string Path,
        string OwnershipName,
        string? NewPath = null,
        string? DisplayName = null,
        string? NewDisplayName = null);

    private readonly SpaceUiMutationDispatcher _uiDispatcher;
    private readonly SpaceRuntimeStateStore _runtimeStore;
    private readonly SpaceDesktopSyncCoordinator _desktopSync;
    private readonly Func<List<SpaceWindow>> _snapshotWindows;
    private readonly Func<bool> _isReloadingFromState;
    private readonly object _gate = new();
    private readonly Queue<DesktopChange> _pendingChanges = new();

    public SpaceDesktopChangeProcessor(
        SpaceUiMutationDispatcher uiDispatcher,
        SpaceRuntimeStateStore runtimeStore,
        SpaceDesktopSyncCoordinator desktopSync,
        Func<List<SpaceWindow>> snapshotWindows,
        Func<bool> isReloadingFromState)
    {
        _uiDispatcher = uiDispatcher;
        _runtimeStore = runtimeStore;
        _desktopSync = desktopSync;
        _snapshotWindows = snapshotWindows;
        _isReloadingFromState = isReloadingFromState;
    }

    internal int PendingCount
    {
        get
        {
            lock (_gate)
                return _pendingChanges.Count;
        }
    }

    public void EnqueueCreated(string path, string? displayName, string ownershipName)
    {
        Enqueue(new DesktopChange(DesktopChangeKind.Created, path, ownershipName, DisplayName: displayName));
    }

    public void EnqueueDeleted(string path, string? displayName, string ownershipName)
    {
        Enqueue(new DesktopChange(DesktopChangeKind.Deleted, path, ownershipName, DisplayName: displayName));
    }

    public void EnqueueRenamed(string oldPath, string newPath, string? oldName, string? newName, string ownershipName)
    {
        Enqueue(new DesktopChange(DesktopChangeKind.Renamed, oldPath, ownershipName, newPath, oldName, newName));
    }

    internal void Enqueue(DesktopChange change)
    {
        EnsureOwnershipName(change.OwnershipName);

        if (_isReloadingFromState())
            return;

        lock (_gate)
            _pendingChanges.Enqueue(change);

        SpaceWindow.TryPostDesktopSync();
    }

    public void ProcessPendingChanges()
    {
        _uiDispatcher.Drain();

        if (_isReloadingFromState())
            return;

        bool changed = false;
        List<SpaceModel> spacesSnapshot = SpaceStateService.Instance.Spaces.ToList();

        while (true)
        {
            DesktopChange? next;
            lock (_gate)
            {
                if (_pendingChanges.Count == 0)
                    break;

                next = _pendingChanges.Dequeue();
            }

            if (next != null && ApplyDesktopChange(next, spacesSnapshot))
                changed = true;
        }

        if (!changed)
            return;

        DesktopOwnershipReconciliationService.ReconciliationResult reconcile =
            DesktopOwnershipReconciliationService.Instance.Reconcile(SpaceStateService.Instance.Spaces);
        if (reconcile.RemovedDuplicateItems > 0)
        {
            Log.Information(
                "SpaceDesktopChangeProcessor: desktop-change reconcile removed {Removed} duplicate item(s)",
                reconcile.RemovedDuplicateItems);
        }

        _desktopSync.ApplyDesktopChangeQueue(_snapshotWindows());
    }

    public void Clear()
    {
        lock (_gate)
            _pendingChanges.Clear();
    }

    internal bool ApplyDesktopChange(DesktopChange change, List<SpaceModel> spacesSnapshot)
    {
        bool changed = false;
        bool knownPath = false;

        foreach (SpaceModel space in spacesSnapshot)
        {
            if (_runtimeStore.IsDeleting(space.Id))
                continue;

            foreach (SpaceItemModel item in space.Items)
            {
                if (!string.Equals(item.TargetPath, change.Path, StringComparison.OrdinalIgnoreCase))
                    continue;

                knownPath = true;

                switch (change.Kind)
                {
                    case DesktopChangeKind.Created:
                        item.IsUnresolved = false;
                        changed = true;
                        break;

                    case DesktopChangeKind.Deleted:
                        item.IsUnresolved = true;
                        changed = true;
                        break;

                    case DesktopChangeKind.Renamed:
                        if (!string.IsNullOrWhiteSpace(change.NewPath))
                        {
                            item.TargetPath = change.NewPath;
                            DesktopEntityRegistryService.Instance.HandleRename(change.Path, change.NewPath, change.NewDisplayName);
                            DesktopEntityModel? renamed = DesktopEntityRegistryService.Instance.TryGetByPath(change.NewPath);
                            if (renamed != null)
                                item.DesktopEntityId = renamed.Id;
                        }

                        if (!string.IsNullOrWhiteSpace(change.NewDisplayName))
                            item.DisplayName = change.NewDisplayName;

                        item.IsUnresolved = false;
                        changed = true;
                        break;
                }
            }
        }

        if (!knownPath && change.Kind == DesktopChangeKind.Created)
        {
            SpaceItemModel? created = _desktopSync.CreateDesktopItemFromPath(change.Path, change.DisplayName);
            if (created != null)
            {
                SpaceModel? targetSpace = ResolveTargetSpaceForNewItem(created);
                if (targetSpace != null && !targetSpace.Items.Any(i => string.Equals(i.TargetPath, created.TargetPath, StringComparison.OrdinalIgnoreCase)))
                {
                    SpaceFileOwnershipService.Instance.EnsureSpaceItemOwnership(targetSpace, created);
                    created.SortOrder = targetSpace.Items.Count;
                    targetSpace.Items.Add(created);
                    changed = true;
                }
            }
        }

        return changed;
    }

    private static void EnsureOwnershipName(string ownershipName)
    {
        if (string.IsNullOrWhiteSpace(ownershipName))
            throw new ArgumentException("Ownership name is required for queue policy enforcement.", nameof(ownershipName));
    }

    private static SpaceModel? ResolveTargetSpaceForNewItem(SpaceItemModel item)
    {
        var spaces = SpaceStateService.Instance.Spaces;
        Guid? targetSpaceId = SpaceIntelligencePipeline.Instance.ResolveTargetSpace(item, spaces);
        if (targetSpaceId.HasValue)
        {
            SpaceModel? matched = SpaceStateService.Instance.GetSpace(targetSpaceId.Value);
            if (matched != null && matched.Type == SpaceType.Standard)
                return matched;
        }

        return spaces.FirstOrDefault(f => f.Type == SpaceType.Standard);
    }
}