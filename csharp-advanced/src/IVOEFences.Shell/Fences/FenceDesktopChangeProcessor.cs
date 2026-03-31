using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceDesktopChangeProcessor
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

    private readonly FenceUiMutationDispatcher _uiDispatcher;
    private readonly FenceRuntimeStateStore _runtimeStore;
    private readonly FenceDesktopSyncCoordinator _desktopSync;
    private readonly Func<List<FenceWindow>> _snapshotWindows;
    private readonly Func<bool> _isReloadingFromState;
    private readonly object _gate = new();
    private readonly Queue<DesktopChange> _pendingChanges = new();

    public FenceDesktopChangeProcessor(
        FenceUiMutationDispatcher uiDispatcher,
        FenceRuntimeStateStore runtimeStore,
        FenceDesktopSyncCoordinator desktopSync,
        Func<List<FenceWindow>> snapshotWindows,
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

        FenceWindow.TryPostDesktopSync();
    }

    public void ProcessPendingChanges()
    {
        _uiDispatcher.Drain();

        if (_isReloadingFromState())
            return;

        bool changed = false;
        List<FenceModel> fencesSnapshot = FenceStateService.Instance.Fences.ToList();

        while (true)
        {
            DesktopChange? next;
            lock (_gate)
            {
                if (_pendingChanges.Count == 0)
                    break;

                next = _pendingChanges.Dequeue();
            }

            if (next != null && ApplyDesktopChange(next, fencesSnapshot))
                changed = true;
        }

        if (!changed)
            return;

        DesktopOwnershipReconciliationService.ReconciliationResult reconcile =
            DesktopOwnershipReconciliationService.Instance.Reconcile(FenceStateService.Instance.Fences);
        if (reconcile.RemovedDuplicateItems > 0)
        {
            Log.Information(
                "FenceDesktopChangeProcessor: desktop-change reconcile removed {Removed} duplicate item(s)",
                reconcile.RemovedDuplicateItems);
        }

        _desktopSync.ApplyDesktopChangeQueue(_snapshotWindows());
    }

    public void Clear()
    {
        lock (_gate)
            _pendingChanges.Clear();
    }

    internal bool ApplyDesktopChange(DesktopChange change, List<FenceModel> fencesSnapshot)
    {
        bool changed = false;
        bool knownPath = false;

        foreach (FenceModel fence in fencesSnapshot)
        {
            if (_runtimeStore.IsDeleting(fence.Id))
                continue;

            foreach (FenceItemModel item in fence.Items)
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
            FenceItemModel? created = _desktopSync.CreateDesktopItemFromPath(change.Path, change.DisplayName);
            if (created != null)
            {
                FenceModel? targetFence = ResolveTargetFenceForNewItem(created);
                if (targetFence != null && !targetFence.Items.Any(i => string.Equals(i.TargetPath, created.TargetPath, StringComparison.OrdinalIgnoreCase)))
                {
                    FenceFileOwnershipService.Instance.EnsureFenceItemOwnership(targetFence, created);
                    created.SortOrder = targetFence.Items.Count;
                    targetFence.Items.Add(created);
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

    private static FenceModel? ResolveTargetFenceForNewItem(FenceItemModel item)
    {
        var fences = FenceStateService.Instance.Fences;
        Guid? targetFenceId = FenceIntelligencePipeline.Instance.ResolveTargetFence(item, fences);
        if (targetFenceId.HasValue)
        {
            FenceModel? matched = FenceStateService.Instance.GetFence(targetFenceId.Value);
            if (matched != null && matched.Type == FenceType.Standard)
                return matched;
        }

        return fences.FirstOrDefault(f => f.Type == FenceType.Standard);
    }
}