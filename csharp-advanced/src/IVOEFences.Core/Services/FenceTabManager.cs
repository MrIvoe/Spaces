using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Manages tab state and switching for multi-page fences.
/// 
/// When a fence has TabContainerId set, it belongs to a tabbed group.
/// This manager handles:
/// - Switching between tabs (smooth animation support)
/// - Tracking active tab index
/// - Providing filtered item list for current tab
/// - Persisting tab state to model
/// </summary>
public sealed class FenceTabManager
{
    private readonly FenceModel _model;
    private int _activeTabIndex;
    private bool _tabsVisible;

    public int ActiveTabIndex
    {
        get => _activeTabIndex;
        set => _activeTabIndex = Math.Max(0, Math.Min(value, _model.Items.Count - 1));
    }

    public bool TabsVisible
    {
        get => _tabsVisible;
        set => _tabsVisible = value;
    }

    /// <summary>Number of tabs (for UI rendering).</summary>
    public int TabCount => _model.Items.Count > 0 ? 1 : 0; // Future: multi-tab support per TabContainerId

    /// <summary>Current active tab name (fence title or custom tab name).</summary>
    public string CurrentTabName => _model.Title;

    /// <summary>Is this fence part of a tabbed container?</summary>
    public bool IsTabbed => _model.TabContainerId.HasValue;

    public FenceTabManager(FenceModel model)
    {
        _model = model;
        _activeTabIndex = model.TabIndex;
        _tabsVisible = IsTabbed;
    }

    /// <summary>Switch to adjacent tab (relative offset: -1 for previous, +1 for next).</summary>
    public bool SwitchTab(int offset)
    {
        if (!IsTabbed) return false;

        int newIndex = _activeTabIndex + offset;
        if (newIndex < 0 || newIndex >= TabCount) return false;

        _activeTabIndex = newIndex;
        _model.TabIndex = _activeTabIndex;
        return true;
    }

    /// <summary>Switch to specific tab index.</summary>
    public bool SetActiveTab(int index)
    {
        if (index < 0 || index >= TabCount) return false;

        _activeTabIndex = index;
        _model.TabIndex = _activeTabIndex;
        return true;
    }

    /// <summary>Toggle tab visibility (for single-tab or multi-tab fences).</summary>
    public void ToggleTabsVisible()
    {
        _tabsVisible = !_tabsVisible;
    }

    /// <summary>Get display name for a tab at given index.</summary>
    public string GetTabName(int index) => index == 0 ? _model.Title : $"Tab {index + 1}";
}
