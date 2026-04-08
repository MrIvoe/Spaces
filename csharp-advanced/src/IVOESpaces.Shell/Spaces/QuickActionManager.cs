using Serilog;

namespace IVOESpaces.Shell.Spaces;

internal sealed class QuickActionManager
{
    public event Action<Guid, string>? ActionTriggered;

    public void AttachActions(SpaceWindow space)
    {
        Log.Debug("QuickActionManager: attached to space {Space}", space.ModelId);
    }

    public void HandleTitleBarRightClick(SpaceWindow space)
    {
        ToggleCollapse(space);
        ActionTriggered?.Invoke(space.ModelId, "Collapse / Expand");

        Log.Information("QuickAction: Collapse / Expand triggered for space {Space}", space.ModelId);
        Log.Information("QuickAction: Rename Space available via RenameSpace(...) hook");
        Log.Information("QuickAction: AI Suggestion available via ShowAISuggestions(...) hook");
    }

    public void ToggleCollapse(SpaceWindow space)
    {
        space.ToggleCollapseExpand();
    }

    public void RenameSpace(SpaceWindow space, string newName)
    {
        if (string.IsNullOrWhiteSpace(newName))
            return;

        space.Rename(newName.Trim());
        ActionTriggered?.Invoke(space.ModelId, "Rename Space");
    }

    public void ShowAISuggestions(SpaceWindow space, Action showSuggestion)
    {
        showSuggestion();
        ActionTriggered?.Invoke(space.ModelId, "AI Suggestion");
    }
}
