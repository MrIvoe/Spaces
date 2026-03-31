namespace IVOEFences.Shell.Fences;

/// <summary>
/// Runtime-only fence state that should not be persisted in FenceModel.
/// </summary>
internal sealed class FenceRuntimeState
{
    public Guid FenceId { get; init; }
    public bool HasUserResized { get; set; }
    public bool IsAnimating { get; set; }
    public bool IsAnimatingMove { get; set; }
    public bool IsAnimatingResize { get; set; }
    public bool IsReanchoring { get; set; }
    public string SearchQuery { get; set; } = string.Empty;
}
