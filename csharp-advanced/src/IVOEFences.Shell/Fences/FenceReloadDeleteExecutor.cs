using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceReloadDeleteExecutor
{
    private readonly FolderPortalService _portalService;
    private readonly FenceRuntimeStateStore _runtimeStore;
    private readonly Func<List<FenceWindow>> _snapshotWindows;
    private readonly Func<int> _getWindowCount;
    private readonly Func<Task> _initializeAsync;
    private readonly Action<FenceWindow> _detachAndDestroyWindow;
    private readonly Action<FenceWindow> _removeWindow;
    private readonly Action _clearWindows;
    private readonly FenceUiMutationDispatcher _uiDispatcher;
    private readonly FenceDesktopChangeProcessor _desktopChangeProcessor;
    private readonly SerialTaskQueue _deleteQueue = new();
    private int _reloadingFromState;

    public FenceReloadDeleteExecutor(
        FolderPortalService portalService,
        FenceRuntimeStateStore runtimeStore,
        Func<List<FenceWindow>> snapshotWindows,
        Func<int> getWindowCount,
        Func<Task> initializeAsync,
        Action<FenceWindow> detachAndDestroyWindow,
        Action<FenceWindow> removeWindow,
        Action clearWindows,
        FenceUiMutationDispatcher uiDispatcher,
        FenceDesktopChangeProcessor desktopChangeProcessor)
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
            "FenceReloadDeleteExecutor: reload starting; windows={WindowCount} pendingDesktopChanges={DesktopChanges} pendingUiMutations={UiMutations}",
            _getWindowCount(),
            _desktopChangeProcessor.PendingCount,
            _uiDispatcher.PendingCount);

        _desktopChangeProcessor.Clear();
        _uiDispatcher.Clear();

        List<FenceWindow> windows = _snapshotWindows();
        foreach (FenceWindow window in windows)
            _portalService.DetachWatcher(window.ModelId);

        foreach (FenceWindow window in windows)
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
            Log.Information("FenceReloadDeleteExecutor: reload completed; windows={WindowCount}", _getWindowCount());
        }
    }

    public void RequestDelete(FenceWindow window)
    {
        _ = _deleteQueue.EnqueueSafe(
            () => DeleteFenceAsync(window),
            ownershipName: $"fence delete {window.ModelId}",
            ex => Log.Error(ex, "FenceReloadDeleteExecutor: delete queue operation failed for fence {FenceId}", window.ModelId));
    }

    public Task AwaitPendingDeleteTasksAsync() => _deleteQueue.WhenIdleAsync();

    private async Task DeleteFenceAsync(FenceWindow window)
    {
        Guid fenceId = window.ModelId;

        if (!_runtimeStore.TryBeginDelete(fenceId))
            return;

        Log.Information("FenceReloadDeleteExecutor: delete pipeline starting for fence {FenceId}", fenceId);

        bool removedFromState = false;
        try
        {
            FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
            if (model == null)
            {
                removedFromState = true;
            }
            else
            {
                await Task.Run(() => ReleaseFenceOwnedItems(model)).ConfigureAwait(false);
                await FenceStateService.Instance.RemoveFenceAsync(model.Id).ConfigureAwait(false);
                removedFromState = true;
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "FenceReloadDeleteExecutor: failed to complete delete pipeline for fence '{FenceId}'", fenceId);
        }

        _uiDispatcher.Enqueue(
            () => FinalizeDeleteOnUi(window, removedFromState),
            ownershipName: $"fence delete finalize {fenceId}");
    }

    private void FinalizeDeleteOnUi(FenceWindow window, bool removedFromState)
    {
        Guid fenceId = window.ModelId;

        try
        {
            if (!removedFromState)
            {
                Log.Warning("FenceReloadDeleteExecutor: delete cancelled for fence '{FenceId}' because state removal did not complete", fenceId);
                return;
            }

            _portalService.DetachWatcher(fenceId);
            _detachAndDestroyWindow(window);
            _removeWindow(window);
            _runtimeStore.RemoveFence(fenceId);
        }
        finally
        {
            _runtimeStore.CompleteDelete(fenceId);
            Log.Information("FenceReloadDeleteExecutor: delete pipeline completed for fence {FenceId} removedFromState={Removed}", fenceId, removedFromState);
        }
    }

    private static void ReleaseFenceOwnedItems(FenceModel model)
    {
        foreach (FenceItemModel item in model.Items)
        {
            try
            {
                FenceFileOwnershipService.Instance.ReleaseFenceItemToDesktop(item);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "FenceReloadDeleteExecutor: failed to release ownership for item '{Path}' in fence '{FenceTitle}'", item.TargetPath, model.Title);
            }
        }
    }
}