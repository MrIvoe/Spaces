using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceTabCoordinator
{
    public bool Merge(Guid primaryFenceId, Guid secondaryFenceId, IEnumerable<FenceWindow> windows)
    {
        if (primaryFenceId == secondaryFenceId)
            return false;

        var state = FenceStateService.Instance;
        FenceModel? primary = state.GetFence(primaryFenceId);
        FenceModel? secondary = state.GetFence(secondaryFenceId);
        if (primary == null || secondary == null)
            return false;

        FenceTabModel? container = TabMergeService.Instance.CreateTabContainer(primary, secondary);
        if (container == null)
            return false;

        foreach (FenceWindow window in windows)
            window.InvalidateContent();

        return true;
    }

    public bool Switch(Guid fenceId, int offset)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        Guid? containerId = model?.TabContainerId;
        if (containerId == null)
            return false;

        FenceTabModel? container = TabMergeService.Instance.GetTabContainer(containerId.Value);
        List<FenceModel> fences = TabMergeService.Instance.GetFencesInContainer(containerId.Value);
        if (container == null || fences.Count < 2)
            return false;

        int newIndex = container.ActiveTabIndex + offset;
        if (newIndex < 0)
            newIndex = fences.Count - 1;
        if (newIndex >= fences.Count)
            newIndex = 0;

        return TabMergeService.Instance.SwitchActiveTab(containerId.Value, newIndex);
    }

    public bool Dissolve(Guid fenceId, IEnumerable<FenceWindow> windows)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        Guid? containerId = model?.TabContainerId;
        if (containerId == null)
            return false;

        bool dissolved = TabMergeService.Instance.DissolveContainer(containerId.Value);
        if (!dissolved)
            return false;

        foreach (FenceWindow window in windows)
            window.InvalidateContent();

        return true;
    }
}
