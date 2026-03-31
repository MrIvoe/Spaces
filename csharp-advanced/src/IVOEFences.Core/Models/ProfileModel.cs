namespace IVOEFences.Core.Models;

public record ProfileModel
{
    public string Id { get; init; } = "default";
    public string Name { get; set; } = "Default";
    public bool IsActive { get; set; }
    public List<Guid> FenceIds { get; set; } = new();
    public List<string> Tags { get; set; } = new();
}
