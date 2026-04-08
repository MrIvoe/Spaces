using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceReloadDeleteExecutor
{
    private readonly FolderPortalService _portalService;
    private readonly SpaceRuntimeStateStore _runtimeStore;
    private readonly Func<List<SpaceWindow>> _snapshotWindows;
    private readonly Func<int> _getWindowCount;
    private readonly Func<Task> _initializeAsync;
    private readonly Action<SpaceWindow> _detachAndDestroyWindow;
    private readonly Action<SpaceWindow> _removeWindow;
    private readonly Action _clearWindows;
    private readonly SpaceUiMutationDispatcher _uiDispatcher;
    private readonly SpaceDesktopChangeProcessor _desktopChangeProcessor;
    private readonly SerialTaskQueue _deleteQueue = new();
    private int _reloadingFromState;

    public SpaceReloadDeleteExecutor(
        FolderPortalService portalService,
        SpaceRuntimeStateStore runtimeStore,
        Func<List<SpaceWindow>> snapshotWindows,
        Func<int> getWindowCount,
        Func<Task> initializeAsync,
        Action<SpaceWindow> detachAndDestroyWindow,
        Action<SpaceWindow> removeWindow,
        Action clearWindows,
        SpaceUiMutationDispatcher uiDispatcher,
        SpaceDesktopChangeProcessor desktopChangeProcessor)
    {
        _portalService = portalService;
        _runtimeStore = runtimeStore;
        _snapshotWindows = snapshotWindows;
        _getWindowCount = getWindowCount;
        _initializeAsync = initializeAsync;
        _detachAndDestroyWindow = detachAndDestroyWindow;
        _removeWindow = removeWindow;
        _clearWindows = clearWindows;
        _uiDispatcher = uiDispatcher;
        _desktopChangeProcessor = desktopChangeProcessor;
    }

    public bool IsReloadingFromState => Volatile.Read(ref _reloadingFromState) == 1;

    internal void SetReloadingFromStateForTesting(bool value)
    {
        Interlocked.Exchange(ref _reloadingFromState, value ? 1 : 0);
    }

    public async Task ReloadFromStateAsync()
    {
        Interlocked.Exchange(ref _reloadingFromState, 1);
        Log.Information(
            "SpaceReloadDeleteExecutor: reload starting; windows={WindowCount} pendingDesktopChanges={DesktopChanges} pendingUiMutations={UiMutations}",
            _getWindowCount(),
            _desktopChangeProcessor.PendingCount,
            _uiDispatcher.PendingCount);

        _desktopChangeProcessor.Clear();
        _uiDispatcher.Clear();

        List<SpaceWindow> windows = _snapshotWindows();
        foreach (SpaceWindow window in windows)
            _portalService.DetachWatcher(window.ModelId);

        foreach (SpaceWindow window in windows)
            _detachAndDestroyWindow(window);

        _clearWindows();
        _runtimeStore.Clear();

        try
        {
            await _initializeAsync().ConfigureAwait(false);
        }
        finally
        {
            Interlocked.Exchange(ref _reloadingFromState, 0);
            Log.Information("SpaceReloadDeleteExecutor: reload completed; windows={WindowCount}", _getWindowCount());
        }
    }

    public void RequestDelete(SpaceWindow window)
    {
        _ = _deleteQueue.EnqueueSafe(
            () => DeleteSpaceAsync(window),
            ownershipName: $"space delete {window.ModelId}",
            ex => Log.Error(ex, "SpaceReloadDeleteExecutor: delete queue operation failed for space {SpaceId}", window.ModelId));
    }

    public Task AwaitPendingDeleteTasksAsync() => _deleteQueue.WhenIdleAsync();

    private async Task DeleteSpaceAsync(SpaceWindow window)
    {
        Guid spaceId = window.ModelId;

        if (!_runtimeStore.TryBeginDelete(spaceId))
            return;

        Log.Information("SpaceReloadDeleteExecutor: delete pipeline starting for space {SpaceId}", spaceId);

        bool removedFromState = false;
        try
        {
            SpaceModel? model = SpaceStateService.Instance.GetSpace(spaceId);
            if (model == null)
            {
                removedFromState = true;
            }
            else
            {
                await Task.Run(() => ReleaseSpaceOwnedItems(model)).ConfigureAwait(false);
                await SpaceStateService.Instance.RemoveSpaceAsync(model.Id).ConfigureAwait(false);
                removedFromState = true;
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "SpaceReloadDeleteExecutor: failed to complete delete pipeline for space '{SpaceId}'", spaceId);
        }

        _uiDispatcher.Enqueue(
            () => FinalizeDeleteOnUi(window, removedFromState),
            ownershipName: $"space delete finalize {spaceId}");
    }

    private void FinalizeDeleteOnUi(SpaceWindow window, bool removedFromState)
    {
        Guid spaceId = window.ModelId;

        try
        {
            if (!removedFromState)
            {
                Log.Warning("SpaceReloadDeleteExecutor: delete cancelled for space '{SpaceId}' because state removal did not complete", spaceId);
                return;
            }

            _portalService.DetachWatcher(spaceId);
            _detachAndDestroyWindow(window);
            _removeWindow(window);
            _runtimeStore.RemoveSpace(spaceId);
        }
        finally
        {
            _runtimeStore.CompleteDelete(spaceId);
            Log.Information("SpaceReloadDeleteExecutor: delete pipeline completed for space {SpaceId} removedFromState={Removed}", spaceId, removedFromState);
        }
    }

    private static void ReleaseSpaceOwnedItems(SpaceModel model)
    {
        foreach (SpaceItemModel item in model.Items)
        {
            try
            {
                SpaceFileOwnershipService.Instance.ReleaseSpaceItemToDesktop(item);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "SpaceReloadDeleteExecutor: failed to release ownership for item '{Path}' in space '{SpaceTitle}'", item.TargetPath, model.Title);
            }
        }
    }
}