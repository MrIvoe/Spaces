namespace IVOEFences.Core.Models;

public record IconMetadata
{
    public Guid ItemId { get; init; }
    public string FileType { get; set; } = string.Empty;
    public int OpenFrequency { get; set; }
    public DateTime? LastOpenedUtc { get; set; }
    public List<string> Tags { get; set; } = new();
    public double AiScore { get; set; }
}
