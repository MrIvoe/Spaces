using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Fences;
using Serilog;

namespace IVOEFences.Shell;

/// <summary>
/// Handles workspace switching and desktop layout cleanup orchestration.
/// </summary>
internal sealed class WorkspaceCoordinator
{
    private readonly FenceManager _fences;
    private readonly PluginLoader _pluginLoader;
    private readonly SerialTaskQueue _workspaceQueue = new();

    public WorkspaceCoordinator(FenceManager fences, PluginLoader pluginLoader)
    {
        _fences = fences;
        _pluginLoader = pluginLoader;
    }

    public void SwitchWorkspaceByIndex(int index)
    {
        _ = _workspaceQueue.EnqueueSafe(
            () => SwitchWorkspaceByIndexAsync(index),
            ownershipName: $"workspace switch index={index}",
            ex => Log.Warning(ex, "WorkspaceCoordinator: queued workspace switch failed for index {Index}", index));
    }

    public async Task SwitchWorkspaceByIndexAsync(int index)
    {
        var profiles = FenceProfileService.Instance.Profiles;
        if (index < 0 || index >= profiles.Count)
        {
            Log.Debug("WorkspaceCoordinator: no workspace at index {Index}", index);
            return;
        }

        string id = profiles[index].Id;
        if (FenceProfileService.Instance.Activate(id))
        {
            var profile = profiles[index];
            await WorkspaceOrchestrator.Instance.ActivateProfileWorkspaceAsync(profile);
            await _fences.ReloadFromStateAsync();
            _pluginLoader.NotifyWorkspaceSwitched(profile.Id);
            Log.Information("WorkspaceCoordinator: switched to workspace '{Name}' (index {Index})",
                profiles[index].Name, index);
        }
    }

    public void CleanUpDesktopLayout()
    {
        _ = _workspaceQueue.EnqueueSafe(
            CleanUpDesktopLayoutAsync,
            ownershipName: "workspace desktop cleanup",
            ex => Log.Warning(ex, "WorkspaceCoordinator: queued desktop cleanup failed"));
    }

    internal Task AwaitPendingWorkspaceTasksAsync() => _workspaceQueue.WhenIdleAsync();

    public async Task CleanUpDesktopLayoutAsync()
    {
        try
        {
            var state = FenceStateService.Instance.Fences.ToList();
            if (state.Count == 0)
                return;

            foreach (var monitorGroup in state.GroupBy(f => f.MonitorDeviceName ?? string.Empty))
            {
                var groupList = monitorGroup.ToList();
                var placements = SmartLayoutEngine.ComputeCleanLayout(groupList);
                var map = placements.ToDictionary(p => p.FenceId, p => p);

                foreach (var fence in groupList)
                {
                    if (!map.TryGetValue(fence.Id, out var p))
                        continue;

                    fence.XFraction = p.X;
                    fence.YFraction = p.Y;
                }
            }

            await FenceStateService.Instance.ReplaceAllAsync(state);
            await _fences.ReloadFromStateAsync();
            Log.Information("WorkspaceCoordinator: desktop cleanup completed for {Count} fences", state.Count);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "WorkspaceCoordinator: desktop cleanup failed");
        }
    }
}
