using Serilog;

namespace IVOEFences.Shell.Fences;

internal sealed class QuickActionManager
{
    public event Action<Guid, string>? ActionTriggered;

    public void AttachActions(FenceWindow fence)
    {
        Log.Debug("QuickActionManager: attached to fence {Fence}", fence.ModelId);
    }

    public void HandleTitleBarRightClick(FenceWindow fence)
    {
        ToggleCollapse(fence);
        ActionTriggered?.Invoke(fence.ModelId, "Collapse / Expand");

        Log.Information("QuickAction: Collapse / Expand triggered for fence {Fence}", fence.ModelId);
        Log.Information("QuickAction: Rename Fence available via RenameFence(...) hook");
        Log.Information("QuickAction: AI Suggestion available via ShowAISuggestions(...) hook");
    }

    public void ToggleCollapse(FenceWindow fence)
    {
        fence.ToggleCollapseExpand();
    }

    public void RenameFence(FenceWindow fence, string newName)
    {
        if (string.IsNullOrWhiteSpace(newName))
            return;

        fence.Rename(newName.Trim());
        ActionTriggered?.Invoke(fence.ModelId, "Rename Fence");
    }

    public void ShowAISuggestions(FenceWindow fence, Action showSuggestion)
    {
        showSuggestion();
        ActionTriggered?.Invoke(fence.ModelId, "AI Suggestion");
    }
}
