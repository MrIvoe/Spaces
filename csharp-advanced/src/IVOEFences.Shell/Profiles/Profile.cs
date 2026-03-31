namespace IVOEFences.Shell.Profiles;

internal sealed record Profile
{
    public string Id { get; init; } = "default";
    public string Name { get; init; } = "Default";
    public bool IsActive { get; set; }
    public List<Guid> ActiveFenceIds { get; init; } = new();
}
