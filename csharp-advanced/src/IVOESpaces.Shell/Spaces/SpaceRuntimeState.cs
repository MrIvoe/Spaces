namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Runtime-only space state that should not be persisted in SpaceModel.
/// </summary>
internal sealed class SpaceRuntimeState
{
    public Guid SpaceId { get; init; }
    public bool HasUserResized { get; set; }
    public bool IsAnimating { get; set; }
    public bool IsAnimatingMove { get; set; }
    public bool IsAnimatingResize { get; set; }
    public bool IsReanchoring { get; set; }
    public string SearchQuery { get; set; } = string.Empty;
}
