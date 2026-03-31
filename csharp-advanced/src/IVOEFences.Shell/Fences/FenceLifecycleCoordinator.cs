using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceLifecycleCoordinator
{
    private readonly FenceUiMutationDispatcher _uiDispatcher;
    private readonly FenceDesktopChangeProcessor _desktopChanges;
    private readonly FenceReloadDeleteExecutor _reloadDeleteExecutor;

    public FenceLifecycleCoordinator(
        FolderPortalService portalService,
        FenceRuntimeStateStore runtimeStore,
        FenceDesktopSyncCoordinator desktopSync,
        Func<List<FenceWindow>> snapshotWindows,
        Func<int> getWindowCount,
        Func<Task> initializeAsync,
        Action<FenceWindow> detachAndDestroyWindow,
        Action<FenceWindow> removeWindow,
        Action clearWindows)
    {
        FenceReloadDeleteExecutor? reloadDeleteExecutor = null;
        _uiDispatcher = new FenceUiMutationDispatcher();
        _desktopChanges = new FenceDesktopChangeProcessor(
            _uiDispatcher,
            runtimeStore,
            desktopSync,
            snapshotWindows,
            () => reloadDeleteExecutor?.IsReloadingFromState ?? false);
        _reloadDeleteExecutor = reloadDeleteExecutor = new FenceReloadDeleteExecutor(
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

    internal void EnqueueDesktopChange(FenceDesktopChangeProcessor.DesktopChange change) => _desktopChanges.Enqueue(change);

    public void ProcessPendingDesktopChanges()
    {
        _desktopChanges.ProcessPendingChanges();
    }

    public Task ReloadFromStateAsync() => _reloadDeleteExecutor.ReloadFromStateAsync();

    public void RequestDelete(FenceWindow window)
    {
        _reloadDeleteExecutor.RequestDelete(window);
    }

    internal bool ApplyDesktopChange(FenceDesktopChangeProcessor.DesktopChange change, List<IVOEFences.Core.Models.FenceModel> fencesSnapshot) =>
        _desktopChanges.ApplyDesktopChange(change, fencesSnapshot);

    internal Task AwaitPendingTasksAsync() => _reloadDeleteExecutor.AwaitPendingDeleteTasksAsync();
}
