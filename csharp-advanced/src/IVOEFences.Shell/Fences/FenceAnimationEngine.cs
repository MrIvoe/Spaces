using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceAnimationEngine
{
    public void OnFenceExpanded(Guid fenceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.FenceExpand, fenceId, null, 220));
    }

    public void OnFenceCollapsed(Guid fenceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.FenceRollup, fenceId, null, 220));
    }

    public void OnIconMoved(Guid fenceId, Guid itemId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.IconMove, fenceId, itemId, 180));
    }

    public void OnFenceResorted(Guid fenceId)
    {
        AnimationFeedbackService.Instance.Request(new AnimationRequest(AnimationKind.ItemHighlight, fenceId, null, 220));
    }
}
