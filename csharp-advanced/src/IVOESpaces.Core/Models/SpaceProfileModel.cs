namespace IVOESpaces.Core.Models;

public enum SpaceProfileTrigger
{
    Manual,
    TimeRange,
    ActiveProcess,
    Peripheral
}

public record SpaceProfileModel
{
    public string Id { get; init; } = "default";
    public string Name { get; set; } = "Default";
    public bool IsBuiltIn { get; set; }
    public bool IsActive { get; set; }

    // Optional activation windows for time-based switching.
    public int ActiveFromHour { get; set; } = 0;
    public int ActiveToHour { get; set; } = 23;

    public SpaceProfileTrigger Trigger { get; set; } = SpaceProfileTrigger.Manual;
    public List<string> TriggerValues { get; set; } = new();

    // Space IDs explicitly visible in this profile.
    public List<Guid> VisibleSpaceIds { get; set; } = new();
}
