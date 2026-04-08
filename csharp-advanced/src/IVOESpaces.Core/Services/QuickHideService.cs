using System;
using System.Collections.Generic;
using System.Linq;
using IVOESpaces.Core.Models;
using IVOESpaces.Shell;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 27: Manages Quick-hide feature (double-click desktop toggles all spaces + icons).
/// When activated: Hides all spaces and unhides all desktop icons.
/// When deactivated: Shows all spaces and hides all desktop icons.
/// 
/// This mimics the original Stardock Spaces behavior where a quick double-click
/// on empty desktop space gives instant access to primary desktop.
/// </summary>
public sealed class QuickHideService
{
    private static QuickHideService? _instance;
    private static readonly object _lock = new();

    private readonly DesktopIconService _iconService;
    private bool _isHiddenState;
    private List<Guid> _hiddenSpaceIds = new();

    public event EventHandler<QuickHideStateChangedEventArgs>? QuickHideStateChanged;

    public static QuickHideService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    _instance ??= new QuickHideService();
                }
            }
            return _instance;
        }
    }

    public QuickHideService()
    {
        _iconService = DesktopIconService.Instance;
        _isHiddenState = false;
        _hiddenSpaceIds = new List<Guid>();
    }

    /// <summary>
    /// Toggles Quick-hide mode: hides all spaces or restores them.
    /// </summary>
    public void ToggleQuickHide()
    {
        if (_isHiddenState)
            RestoreAllSpaces();
        else
            HideAllSpaces();
    }

    /// <summary>
    /// Hides all spaces and unhides all desktop icons.
    /// </summary>
    private void HideAllSpaces()
    {
        if (_isHiddenState)
            return;

        try
        {
            var stateService = SpaceStateService.Instance;
            var all = stateService.Spaces;
            _hiddenSpaceIds.Clear();

            // Hide all spaces (track which ones were visible before hiding)
            foreach (var space in all)
            {
                if (!space.IsHidden)
                {
                    _hiddenSpaceIds.Add(space.Id);
                    space.IsHidden = true;
                }
            }

            stateService.MarkDirty();

            // Unhide all desktop icons (clear LVIS_CUT state for all items)
            UnhideAllDesktopIcons();

            _isHiddenState = true;

            QuickHideStateChanged?.Invoke(this, new QuickHideStateChangedEventArgs
            {
                IsHidden = true,
                SpaceCount = _hiddenSpaceIds.Count
            });

            Serilog.Log.Information("Quick-hide activated: {Count} spaces hidden", _hiddenSpaceIds.Count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to activate Quick-hide");
        }
    }

    /// <summary>
    /// Restores all previously hidden spaces and hides desktop icons again.
    /// </summary>
    private void RestoreAllSpaces()
    {
        if (!_isHiddenState)
            return;

        try
        {
            var stateService = SpaceStateService.Instance;

            // Unhide spaces that were hidden by Quick-hide
            foreach (var space in stateService.Spaces)
            {
                if (_hiddenSpaceIds.Contains(space.Id))
                {
                    space.IsHidden = false;
                }
            }

            stateService.MarkDirty();

            // Re-hide all desktop icons in space items (set LVIS_CUT state)
            HideAllDesktopIcons();

            _isHiddenState = false;
            var count = _hiddenSpaceIds.Count;
            _hiddenSpaceIds.Clear();

            QuickHideStateChanged?.Invoke(this, new QuickHideStateChangedEventArgs
            {
                IsHidden = false,
                SpaceCount = count
            });

            Serilog.Log.Information("Quick-hide deactivated: {Count} spaces restored", count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to deactivate Quick-hide");
        }
    }

    /// <summary>
    /// Gets whether Quick-hide is currently active.
    /// </summary>
    public bool IsQuickHideActive => _isHiddenState;

    private void HideAllDesktopIcons()
    {
        try
        {
            foreach (var space in SpaceStateService.Instance.Spaces)
            {
                foreach (var item in space.Items)
                {
                    _iconService.HideItem(item.DisplayName);
                }
            }

            Serilog.Log.Debug("All desktop icons hidden (set LVIS_CUT)");
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to hide desktop icons");
        }
    }

    private void UnhideAllDesktopIcons()
    {
        try
        {
            foreach (var space in SpaceStateService.Instance.Spaces)
            {
                foreach (var item in space.Items)
                {
                    _iconService.ShowItem(item.DisplayName);
                }
            }

            Serilog.Log.Debug("All desktop icons shown (cleared LVIS_CUT)");
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to unhide desktop icons");
        }
    }
}

/// <summary>
/// Event args for Quick-hide state changes.
/// </summary>
public sealed class QuickHideStateChangedEventArgs : EventArgs
{
    public bool IsHidden { get; set; }
    public int SpaceCount { get; set; }
}
