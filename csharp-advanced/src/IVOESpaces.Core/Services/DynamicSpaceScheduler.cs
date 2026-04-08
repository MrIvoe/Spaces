using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public sealed class DynamicSpaceScheduler
{
    private static readonly Lazy<DynamicSpaceScheduler> _instance = new(() => new DynamicSpaceScheduler());
    public static DynamicSpaceScheduler Instance => _instance.Value;

    private DynamicSpaceScheduler()
    {
    }

    public void ApplyTimeWindowVisibility(DateTime nowLocal)
    {
        if (!AppSettingsRepository.Instance.Current.EnableDynamicSpaces)
            return;

        foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
        {
            if (!space.IsDynamicVisibilityEnabled)
                continue;

            bool visible = IsHourInRange(nowLocal.Hour, space.VisibleFromHour, space.VisibleToHour);
            space.IsHidden = !visible;
        }

        SpaceStateService.Instance.MarkDirty();
    }

    public void ApplyProfileVisibility(SpaceProfileModel profile)
    {
        var visible = profile.VisibleSpaceIds.ToHashSet();

        foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
        {
            if (!visible.Contains(space.Id) && profile.VisibleSpaceIds.Count > 0)
                space.IsHidden = true;
            else if (profile.VisibleSpaceIds.Count > 0)
                space.IsHidden = false;
        }

        SpaceStateService.Instance.MarkDirty();
    }

    private static bool IsHourInRange(int currentHour, int startHour, int endHour)
    {
        if (startHour <= endHour)
            return currentHour >= startHour && currentHour <= endHour;

        return currentHour >= startHour || currentHour <= endHour;
    }
}
