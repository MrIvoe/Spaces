namespace IVOEFences.Core.Models;

public enum FenceProfileTrigger
{
    Manual,
    TimeRange,
    ActiveProcess,
    Peripheral
}

public record FenceProfileModel
{
    public string Id { get; init; } = "default";
    public string Name { get; set; } = "Default";
    public bool IsBuiltIn { get; set; }
    public bool IsActive { get; set; }

    // Optional activation windows for time-based switching.
    public int ActiveFromHour { get; set; } = 0;
    public int ActiveToHour { get; set; } = 23;

    public FenceProfileTrigger Trigger { get; set; } = FenceProfileTrigger.Manual;
    public List<string> TriggerValues { get; set; } = new();

    // Fence IDs explicitly visible in this profile.
    public List<Guid> VisibleFenceIds { get; set; } = new();
}
