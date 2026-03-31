using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class DynamicFenceScheduler
{
    private static readonly Lazy<DynamicFenceScheduler> _instance = new(() => new DynamicFenceScheduler());
    public static DynamicFenceScheduler Instance => _instance.Value;

    private DynamicFenceScheduler()
    {
    }

    public void ApplyTimeWindowVisibility(DateTime nowLocal)
    {
        if (!AppSettingsRepository.Instance.Current.EnableDynamicFences)
            return;

        foreach (FenceModel fence in FenceStateService.Instance.Fences)
        {
            if (!fence.IsDynamicVisibilityEnabled)
                continue;

            bool visible = IsHourInRange(nowLocal.Hour, fence.VisibleFromHour, fence.VisibleToHour);
            fence.IsHidden = !visible;
        }

        FenceStateService.Instance.MarkDirty();
    }

    public void ApplyProfileVisibility(FenceProfileModel profile)
    {
        var visible = profile.VisibleFenceIds.ToHashSet();

        foreach (FenceModel fence in FenceStateService.Instance.Fences)
        {
            if (!visible.Contains(fence.Id) && profile.VisibleFenceIds.Count > 0)
                fence.IsHidden = true;
            else if (profile.VisibleFenceIds.Count > 0)
                fence.IsHidden = false;
        }

        FenceStateService.Instance.MarkDirty();
    }

    private static bool IsHourInRange(int currentHour, int startHour, int endHour)
    {
        if (startHour <= endHour)
            return currentHour >= startHour && currentHour <= endHour;

        return currentHour >= startHour || currentHour <= endHour;
    }
}
