namespace IVOEFences.Core.Services;

public enum AnimationKind
{
    FenceRollup,
    FenceExpand,
    IconMove,
    TabSwitch,
    SuggestionPulse,
    ItemHighlight
}

public sealed record AnimationRequest(AnimationKind Kind, Guid? FenceId, Guid? ItemId, int DurationMs);

public sealed class AnimationFeedbackService
{
    private static readonly Lazy<AnimationFeedbackService> _instance = new(() => new AnimationFeedbackService());
    public static AnimationFeedbackService Instance => _instance.Value;

    public event EventHandler<AnimationRequest>? AnimationRequested;

    private AnimationFeedbackService()
    {
    }

    public void Request(AnimationRequest request)
    {
        AnimationRequested?.Invoke(this, request);
    }

    public void PulseSuggestion(Guid fenceId, Guid itemId)
    {
        Request(new AnimationRequest(AnimationKind.SuggestionPulse, fenceId, itemId, 360));
    }
}
