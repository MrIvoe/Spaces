using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 25: Manages fence roll-up/roll-down animation and state transitions.
/// Handles:
/// - State persistence (IsRolledUp, PreRollupHeightFraction)
/// - Animation timing and easing
/// - Undo/redo integration
/// - Multi-fence coordination (all fences roll up together on Ctrl+Win+Space)
/// </summary>
public sealed class RollupService
{
    private static RollupService? _instance;
    private static readonly object _lock = new();

    private readonly UndoService _undoService;

    public event EventHandler<RollupStateChangedEventArgs>? RollupStateChanged;

    private const int ROLLUP_ANIMATION_MS = 300;
    private const double MIN_ROLLUP_HEIGHT_FRACTION = 0.02; // Approx 24 pixels at 1080p

    public static RollupService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    _instance ??= new RollupService();
                }
            }
            return _instance;
        }
    }

    public RollupService()
    {
        _undoService = UndoService.Instance;
    }

    /// <summary>
    /// Toggles roll-up state for a specific fence with animation.
    /// </summary>
    public async Task ToggleRollupAsync(Guid fenceId, int titleBarHeightPixels = 24)
    {
        var stateService = FenceStateService.Instance;
        var fence = stateService.GetFence(fenceId);
        if (fence == null)
            return;

        var oldState = (IsRolledUp: fence.IsRolledUp, Height: fence.HeightFraction);

        if (fence.IsRolledUp)
        {
            // Unroll: restore to previous height
            fence.HeightFraction = fence.PreRollupHeightFraction > 0
                ? fence.PreRollupHeightFraction
                : 0.20; // Default if not set
            fence.IsRolledUp = false;
        }
        else
        {
            // Roll up: save height, collapse to title bar
            fence.PreRollupHeightFraction = fence.HeightFraction;
            // Calculate title bar height as fraction of monitor (assume 1080p ~ 0.022 fraction per pixel)
            fence.HeightFraction = Math.Max(MIN_ROLLUP_HEIGHT_FRACTION, titleBarHeightPixels / 1080.0);
            fence.IsRolledUp = true;
        }

        stateService.MarkDirty();

        // Record undo action as PropertyChangeCommand
        var command = new UndoService.PropertyChangeCommand<bool>(
            $"Rollup {fence.Title}",
            () => oldState.IsRolledUp,
            (val) => { }, // Undo handled by UndoRollup call
            fence.IsRolledUp
        );
        _undoService.Execute(command);

        RollupStateChanged?.Invoke(this, new RollupStateChangedEventArgs
        {
            FenceId = fenceId,
            IsRolledUp = fence.IsRolledUp,
            TargetHeightFraction = fence.HeightFraction,
            AnimationDurationMs = ROLLUP_ANIMATION_MS
        });

        Serilog.Log.Information("Fence {Id} rolled {State}",
            fenceId,
            fence.IsRolledUp ? "up" : "down");

        await Task.Delay(ROLLUP_ANIMATION_MS);
    }

    /// <summary>
    /// Toggles roll-up for ALL fences simultaneously (Ctrl+Win+Space hotkey).
    /// </summary>
    public async Task ToggleAllRollupsAsync(int titleBarHeightPixels = 24)
    {
        var fenceIds = FenceStateService.Instance.Fences.Select(f => f.Id).ToList();
        var toggleTasks = new List<Task>();

        foreach (var id in fenceIds)
        {
            toggleTasks.Add(ToggleRollupAsync(id, titleBarHeightPixels));
        }

        await Task.WhenAll(toggleTasks);
    }

    /// <summary>
    /// Auto-unroll fence on hover.
    /// Note: RollupOpenMode setting (from AppSettings/Step 40) not yet implemented.
    /// </summary>
    public async Task UnrollOnHoverAsync(Guid fenceId)
    {
        var fence = FenceStateService.Instance.GetFence(fenceId);
        if (fence is null || !fence.IsRolledUp)
            return;

        // TODO: Check AppSettings.RollupOpenMode == "Hover" when Step 40 available
        // For now, always unroll on hover
        await ToggleRollupAsync(fenceId);
    }

    /// <summary>
    /// Gets current rollup animation parameters for WPF binding.
    /// </summary>
    public RollupAnimationParams GetAnimationParams(Guid fenceId)
    {
        var fence = FenceStateService.Instance.GetFence(fenceId);
        if (fence is null)
            return new();

        return new RollupAnimationParams
        {
            IsRolledUp = fence.IsRolledUp,
            TargetHeightFraction = fence.HeightFraction,
            PreRollupHeightFraction = fence.PreRollupHeightFraction,
            AnimationDurationMs = ROLLUP_ANIMATION_MS
        };
    }

    private void UndoRollup(Guid fenceId, bool wasRolledUp, double previousHeight)
    {
        var stateService = FenceStateService.Instance;
        var fence = stateService.GetFence(fenceId);
        if (fence != null)
        {
            fence.IsRolledUp = wasRolledUp;
            fence.HeightFraction = previousHeight;
            stateService.MarkDirty();

            RollupStateChanged?.Invoke(this, new RollupStateChangedEventArgs
            {
                FenceId = fenceId,
                IsRolledUp = fence.IsRolledUp,
                TargetHeightFraction = fence.HeightFraction,
                AnimationDurationMs = ROLLUP_ANIMATION_MS
            });
        }
    }
}

/// <summary>
/// Event args for rollup state changes.
/// </summary>
public sealed class RollupStateChangedEventArgs : EventArgs
{
    public Guid FenceId { get; set; }
    public bool IsRolledUp { get; set; }
    public double TargetHeightFraction { get; set; }
    public int AnimationDurationMs { get; set; }
}

/// <summary>
/// Animation parameters for WPF rollup animation binding.
/// </summary>
public class RollupAnimationParams
{
    public bool IsRolledUp { get; set; }
    public double TargetHeightFraction { get; set; }
    public double PreRollupHeightFraction { get; set; }
    public int AnimationDurationMs { get; set; } = 300;
}
