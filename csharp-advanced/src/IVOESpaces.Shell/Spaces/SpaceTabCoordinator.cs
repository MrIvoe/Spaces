using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceTabCoordinator
{
    public bool Merge(Guid primarySpaceId, Guid secondarySpaceId, IEnumerable<SpaceWindow> windows)
    {
        if (primarySpaceId == secondarySpaceId)
            return false;

        var state = SpaceStateService.Instance;
        SpaceModel? primary = state.GetSpace(primarySpaceId);
        SpaceModel? secondary = state.GetSpace(secondarySpaceId);
        if (primary == null || secondary == null)
            return false;

        SpaceTabModel? container = TabMergeService.Instance.CreateTabContainer(primary, secondary);
        if (container == null)
            return false;

        foreach (SpaceWindow window in windows)
            window.InvalidateContent();

        return true;
    }

    public bool Switch(Guid spaceId, int offset)
    {
        SpaceModel? model = SpaceStateService.Instance.GetSpace(spaceId);
        Guid? containerId = model?.TabContainerId;
        if (containerId == null)
            return false;

        SpaceTabModel? container = TabMergeService.Instance.GetTabContainer(containerId.Value);
        List<SpaceModel> spaces = TabMergeService.Instance.GetSpacesInContainer(containerId.Value);
        if (container == null || spaces.Count < 2)
            return false;

        int newIndex = container.ActiveTabIndex + offset;
        if (newIndex < 0)
            newIndex = spaces.Count - 1;
        if (newIndex >= spaces.Count)
            newIndex = 0;

        return TabMergeService.Instance.SwitchActiveTab(containerId.Value, newIndex);
    }

    public bool Dissolve(Guid spaceId, IEnumerable<SpaceWindow> windows)
    {
        SpaceModel? model = SpaceStateService.Instance.GetSpace(spaceId);
        Guid? containerId = model?.TabContainerId;
        if (containerId == null)
            return false;

        bool dissolved = TabMergeService.Instance.DissolveContainer(containerId.Value);
        if (!dissolved)
            return false;

        foreach (SpaceWindow window in windows)
            window.InvalidateContent();

        return true;
    }
}
