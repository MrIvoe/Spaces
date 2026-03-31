using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;
using IVOEFences.Shell;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 27: Manages Quick-hide feature (double-click desktop toggles all fences + icons).
/// When activated: Hides all fences and unhides all desktop icons.
/// When deactivated: Shows all fences and hides all desktop icons.
/// 
/// This mimics the original Stardock Fences behavior where a quick double-click
/// on empty desktop space gives instant access to primary desktop.
/// </summary>
public sealed class QuickHideService
{
    private static QuickHideService? _instance;
    private static readonly object _lock = new();

    private readonly DesktopIconService _iconService;
    private bool _isHiddenState;
    private List<Guid> _hiddenFenceIds = new();

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
        _hiddenFenceIds = new List<Guid>();
    }

    /// <summary>
    /// Toggles Quick-hide mode: hides all fences or restores them.
    /// </summary>
    public void ToggleQuickHide()
    {
        if (_isHiddenState)
            RestoreAllFences();
        else
            HideAllFences();
    }

    /// <summary>
    /// Hides all fences and unhides all desktop icons.
    /// </summary>
    private void HideAllFences()
    {
        if (_isHiddenState)
            return;

        try
        {
            var stateService = FenceStateService.Instance;
            var all = stateService.Fences;
            _hiddenFenceIds.Clear();

            // Hide all fences (track which ones were visible before hiding)
            foreach (var fence in all)
            {
                if (!fence.IsHidden)
                {
                    _hiddenFenceIds.Add(fence.Id);
                    fence.IsHidden = true;
                }
            }

            stateService.MarkDirty();

            // Unhide all desktop icons (clear LVIS_CUT state for all items)
            UnhideAllDesktopIcons();

            _isHiddenState = true;

            QuickHideStateChanged?.Invoke(this, new QuickHideStateChangedEventArgs
            {
                IsHidden = true,
                FenceCount = _hiddenFenceIds.Count
            });

            Serilog.Log.Information("Quick-hide activated: {Count} fences hidden", _hiddenFenceIds.Count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to activate Quick-hide");
        }
    }

    /// <summary>
    /// Restores all previously hidden fences and hides desktop icons again.
    /// </summary>
    private void RestoreAllFences()
    {
        if (!_isHiddenState)
            return;

        try
        {
            var stateService = FenceStateService.Instance;

            // Unhide fences that were hidden by Quick-hide
            foreach (var fence in stateService.Fences)
            {
                if (_hiddenFenceIds.Contains(fence.Id))
                {
                    fence.IsHidden = false;
                }
            }

            stateService.MarkDirty();

            // Re-hide all desktop icons in fence items (set LVIS_CUT state)
            HideAllDesktopIcons();

            _isHiddenState = false;
            var count = _hiddenFenceIds.Count;
            _hiddenFenceIds.Clear();

            QuickHideStateChanged?.Invoke(this, new QuickHideStateChangedEventArgs
            {
                IsHidden = false,
                FenceCount = count
            });

            Serilog.Log.Information("Quick-hide deactivated: {Count} fences restored", count);
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
            foreach (var fence in FenceStateService.Instance.Fences)
            {
                foreach (var item in fence.Items)
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
            foreach (var fence in FenceStateService.Instance.Fences)
            {
                foreach (var item in fence.Items)
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
    public int FenceCount { get; set; }
}
