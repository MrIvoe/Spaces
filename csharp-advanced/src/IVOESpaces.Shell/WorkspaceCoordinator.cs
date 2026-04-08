using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Spaces;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Handles workspace switching and desktop layout cleanup orchestration.
/// </summary>
internal sealed class WorkspaceCoordinator
{
    private readonly SpaceManager _spaces;
    private readonly PluginLoader _pluginLoader;
    private readonly SerialTaskQueue _workspaceQueue = new();

    public WorkspaceCoordinator(SpaceManager spaces, PluginLoader pluginLoader)
    {
        _spaces = spaces;
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
        var profiles = SpaceProfileService.Instance.Profiles;
        if (index < 0 || index >= profiles.Count)
        {
            Log.Debug("WorkspaceCoordinator: no workspace at index {Index}", index);
            return;
        }

        string id = profiles[index].Id;
        if (SpaceProfileService.Instance.Activate(id))
        {
            var profile = profiles[index];
            await WorkspaceOrchestrator.Instance.ActivateProfileWorkspaceAsync(profile);
            await _spaces.ReloadFromStateAsync();
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
            var state = SpaceStateService.Instance.Spaces.ToList();
            if (state.Count == 0)
                return;

            foreach (var monitorGroup in state.GroupBy(f => f.MonitorDeviceName ?? string.Empty))
            {
                var groupList = monitorGroup.ToList();
                var placements = SmartLayoutEngine.ComputeCleanLayout(groupList);
                var map = placements.ToDictionary(p => p.SpaceId, p => p);

                foreach (var space in groupList)
                {
                    if (!map.TryGetValue(space.Id, out var p))
                        continue;

                    space.XFraction = p.X;
                    space.YFraction = p.Y;
                }
            }

            await SpaceStateService.Instance.ReplaceAllAsync(state);
            await _spaces.ReloadFromStateAsync();
            Log.Information("WorkspaceCoordinator: desktop cleanup completed for {Count} spaces", state.Count);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "WorkspaceCoordinator: desktop cleanup failed");
        }
    }
}
