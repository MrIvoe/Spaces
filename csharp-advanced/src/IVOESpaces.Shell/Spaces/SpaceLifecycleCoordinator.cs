using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceLifecycleCoordinator
{
    private readonly SpaceUiMutationDispatcher _uiDispatcher;
    private readonly SpaceDesktopChangeProcessor _desktopChanges;
    private readonly SpaceReloadDeleteExecutor _reloadDeleteExecutor;

    public SpaceLifecycleCoordinator(
        FolderPortalService portalService,
        SpaceRuntimeStateStore runtimeStore,
        SpaceDesktopSyncCoordinator desktopSync,
        Func<List<SpaceWindow>> snapshotWindows,
        Func<int> getWindowCount,
        Func<Task> initializeAsync,
        Action<SpaceWindow> detachAndDestroyWindow,
        Action<SpaceWindow> removeWindow,
        Action clearWindows)
    {
        SpaceReloadDeleteExecutor? reloadDeleteExecutor = null;
        _uiDispatcher = new SpaceUiMutationDispatcher();
        _desktopChanges = new SpaceDesktopChangeProcessor(
            _uiDispatcher,
            runtimeStore,
            desktopSync,
            snapshotWindows,
            () => reloadDeleteExecutor?.IsReloadingFromState ?? false);
        _reloadDeleteExecutor = reloadDeleteExecutor = new SpaceReloadDeleteExecutor(
            portalService,
            runtimeStore,
            snapshotWindows,
            getWindowCount,
            initializeAsync,
            detachAndDestroyWindow,
            removeWindow,
            clearWindows,
            _uiDispatcher,
            _desktopChanges);
    }

    public bool IsReloadingFromState => _reloadDeleteExecutor.IsReloadingFromState;

    internal int PendingDesktopChangeCount => _desktopChanges.PendingCount;

    internal int PendingUiMutationCount => _uiDispatcher.PendingCount;

    internal void SetReloadingFromStateForTesting(bool value)
    {
        _reloadDeleteExecutor.SetReloadingFromStateForTesting(value);
    }

    public void EnqueueUiMutation(Action action, string ownershipName)
    {
        _uiDispatcher.Enqueue(action, ownershipName);
    }

    public void DrainUiMutations()
    {
        _uiDispatcher.Drain();
    }

    public void EnqueueDesktopCreated(string path, string? displayName, string ownershipName)
    {
        _desktopChanges.EnqueueCreated(path, displayName, ownershipName);
    }

    public void EnqueueDesktopDeleted(string path, string? displayName, string ownershipName)
    {
        _desktopChanges.EnqueueDeleted(path, displayName, ownershipName);
    }

    public void EnqueueDesktopRenamed(string oldPath, string newPath, string? oldName, string? newName, string ownershipName)
    {
        _desktopChanges.EnqueueRenamed(oldPath, newPath, oldName, newName, ownershipName);
    }

    internal void EnqueueDesktopChange(SpaceDesktopChangeProcessor.DesktopChange change) => _desktopChanges.Enqueue(change);

    public void ProcessPendingDesktopChanges()
    {
        _desktopChanges.ProcessPendingChanges();
    }

    public Task ReloadFromStateAsync() => _reloadDeleteExecutor.ReloadFromStateAsync();

    public void RequestDelete(SpaceWindow window)
    {
        _reloadDeleteExecutor.RequestDelete(window);
    }

    internal bool ApplyDesktopChange(SpaceDesktopChangeProcessor.DesktopChange change, List<IVOESpaces.Core.Models.SpaceModel> spacesSnapshot) =>
        _desktopChanges.ApplyDesktopChange(change, spacesSnapshot);

    internal Task AwaitPendingTasksAsync() => _reloadDeleteExecutor.AwaitPendingDeleteTasksAsync();
}
