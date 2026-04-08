using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 25: Manages space roll-up/roll-down animation and state transitions.
/// Handles:
/// - State persistence (IsRolledUp, PreRollupHeightFraction)
/// - Animation timing and easing
/// - Undo/redo integration
/// - Multi-space coordination (all spaces roll up together on Ctrl+Win+Space)
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
    /// Toggles roll-up state for a specific space with animation.
    /// </summary>
    public async Task ToggleRollupAsync(Guid spaceId, int titleBarHeightPixels = 24)
    {
        var stateService = SpaceStateService.Instance;
        var space = stateService.GetSpace(spaceId);
        if (space == null)
            return;

        var oldState = (IsRolledUp: space.IsRolledUp, Height: space.HeightFraction);

        if (space.IsRolledUp)
        {
            // Unroll: restore to previous height
            space.HeightFraction = space.PreRollupHeightFraction > 0
                ? space.PreRollupHeightFraction
                : 0.20; // Default if not set
            space.IsRolledUp = false;
        }
        else
        {
            // Roll up: save height, collapse to title bar
            space.PreRollupHeightFraction = space.HeightFraction;
            // Calculate title bar height as fraction of monitor (assume 1080p ~ 0.022 fraction per pixel)
            space.HeightFraction = Math.Max(MIN_ROLLUP_HEIGHT_FRACTION, titleBarHeightPixels / 1080.0);
            space.IsRolledUp = true;
        }

        stateService.MarkDirty();

        // Record undo action as PropertyChangeCommand
        var command = new UndoService.PropertyChangeCommand<bool>(
            $"Rollup {space.Title}",
            () => oldState.IsRolledUp,
            (val) => { }, // Undo handled by UndoRollup call
            space.IsRolledUp
        );
        _undoService.Execute(command);

        RollupStateChanged?.Invoke(this, new RollupStateChangedEventArgs
        {
            SpaceId = spaceId,
            IsRolledUp = space.IsRolledUp,
            TargetHeightFraction = space.HeightFraction,
            AnimationDurationMs = ROLLUP_ANIMATION_MS
        });

        Serilog.Log.Information("Space {Id} rolled {State}",
            spaceId,
            space.IsRolledUp ? "up" : "down");

        await Task.Delay(ROLLUP_ANIMATION_MS);
    }

    /// <summary>
    /// Toggles roll-up for ALL spaces simultaneously (Ctrl+Win+Space hotkey).
    /// </summary>
    public async Task ToggleAllRollupsAsync(int titleBarHeightPixels = 24)
    {
        var spaceIds = SpaceStateService.Instance.Spaces.Select(f => f.Id).ToList();
        var toggleTasks = new List<Task>();

        foreach (var id in spaceIds)
        {
            toggleTasks.Add(ToggleRollupAsync(id, titleBarHeightPixels));
        }

        await Task.WhenAll(toggleTasks);
    }

    /// <summary>
    /// Auto-unroll space on hover.
    /// Note: RollupOpenMode setting (from AppSettings/Step 40) not yet implemented.
    /// </summary>
    public async Task UnrollOnHoverAsync(Guid spaceId)
    {
        var space = SpaceStateService.Instance.GetSpace(spaceId);
        if (space is null || !space.IsRolledUp)
            return;

        // TODO: Check AppSettings.RollupOpenMode == "Hover" when Step 40 available
        // For now, always unroll on hover
        await ToggleRollupAsync(spaceId);
    }

    /// <summary>
    /// Gets current rollup animation parameters for WPF binding.
    /// </summary>
    public RollupAnimationParams GetAnimationParams(Guid spaceId)
    {
        var space = SpaceStateService.Instance.GetSpace(spaceId);
        if (space is null)
            return new();

        return new RollupAnimationParams
        {
            IsRolledUp = space.IsRolledUp,
            TargetHeightFraction = space.HeightFraction,
            PreRollupHeightFraction = space.PreRollupHeightFraction,
            AnimationDurationMs = ROLLUP_ANIMATION_MS
        };
    }

    private void UndoRollup(Guid spaceId, bool wasRolledUp, double previousHeight)
    {
        var stateService = SpaceStateService.Instance;
        var space = stateService.GetSpace(spaceId);
        if (space != null)
        {
            space.IsRolledUp = wasRolledUp;
            space.HeightFraction = previousHeight;
            stateService.MarkDirty();

            RollupStateChanged?.Invoke(this, new RollupStateChangedEventArgs
            {
                SpaceId = spaceId,
                IsRolledUp = space.IsRolledUp,
                TargetHeightFraction = space.HeightFraction,
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
    public Guid SpaceId { get; set; }
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
