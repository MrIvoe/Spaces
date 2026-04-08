namespace IVOESpaces.Core.Services;

public enum AnimationKind
{
    SpaceRollup,
    SpaceExpand,
    IconMove,
    TabSwitch,
    SuggestionPulse,
    ItemHighlight
}

public sealed record AnimationRequest(AnimationKind Kind, Guid? SpaceId, Guid? ItemId, int DurationMs);

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

    public void PulseSuggestion(Guid spaceId, Guid itemId)
    {
        Request(new AnimationRequest(AnimationKind.SuggestionPulse, spaceId, itemId, 360));
    }
}
