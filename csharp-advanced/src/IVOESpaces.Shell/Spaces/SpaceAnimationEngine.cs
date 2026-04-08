using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceAnimationEngine
{
    public void OnSpaceExpanded(Guid spaceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.SpaceExpand, spaceId, null, 220));
    }

    public void OnSpaceCollapsed(Guid spaceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.SpaceRollup, spaceId, null, 220));
    }

    public void OnIconMoved(Guid spaceId, Guid itemId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.IconMove, spaceId, itemId, 180));
    }

    public void OnSpaceResorted(Guid spaceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.ItemHighlight, spaceId, null, 220));
    }
}
