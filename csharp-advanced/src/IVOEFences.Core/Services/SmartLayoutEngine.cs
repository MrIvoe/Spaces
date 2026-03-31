using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Computes a clean, non-overlapping, grid-aligned layout for all fences.
///
/// Usage:
///   var placements = SmartLayoutEngine.ComputeCleanLayout(fences);
///   foreach (var p in placements)
///   {
///       var fence = state.GetFence(p.FenceId)!;
///       fence.XFraction = p.X;
///       fence.YFraction = p.Y;
///   }
///   await fenceRepository.SaveAsync(state.Fences);
/// </summary>
public static class SmartLayoutEngine
{
    /// <summary>Grid resolution as a fraction of the display work area (2%).</summary>
    public const double GridStep = 0.02;

    /// <summary>Minimum gap between fences, expressed in grid steps.</summary>
    private const double GapFraction = GridStep;

    public readonly record struct FencePlacement(Guid FenceId, double X, double Y);

    /// <summary>
    /// Computes cleaned-up positions for all <paramref name="fences"/> on a single monitor.
    ///
    /// Rules:
    /// - Positions are snapped to a <see cref="GridStep"/> grid.
    /// - No two fences overlap (a <see cref="GapFraction"/> gap is enforced).
    /// - Fences stay within [0, <paramref name="maxX"/>] × [0, <paramref name="maxY"/>].
    /// - Width/height of each fence is preserved; only X/Y are adjusted.
    /// - Reading order (top-to-bottom, left-to-right) is respected for placement priority.
    /// </summary>
    public static IReadOnlyList<FencePlacement> ComputeCleanLayout(
        IReadOnlyList<FenceModel> fences,
        double maxX = 1.0,
        double maxY = 1.0)
    {
        if (fences.Count == 0)
            return Array.Empty<FencePlacement>();

        // Prioritise existing reading order (top row first, then left → right)
        var sorted = fences
            .OrderBy(f => Snap(f.YFraction))
            .ThenBy(f => Snap(f.XFraction))
            .ToList();

        var placements = new List<FencePlacement>(sorted.Count);
        var placed     = new List<PlacedRect>();

        foreach (var fence in sorted)
        {
            double w = fence.WidthFraction;
            double h = fence.HeightFraction;

            // Clamp size so it can actually fit
            w = Math.Min(w, maxX);
            h = Math.Min(h, maxY);

            double snapX = Snap(fence.XFraction);
            double snapY = Snap(fence.YFraction);

            (double fx, double fy) = FindFreeSlot(snapX, snapY, w, h, placed, maxX, maxY);

            placements.Add(new FencePlacement(fence.Id, fx, fy));
            placed.Add(new PlacedRect(fx, fy, w, h));
        }

        return placements;
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    private readonly record struct PlacedRect(double X, double Y, double W, double H);

    /// <summary>
    /// Searches for the nearest grid slot starting at (<paramref name="startX"/>,
    /// <paramref name="startY"/>) that does not overlap any already-placed fence.
    /// Scans across (X first) then down (Y), wrapping to X=0 when a new row is started.
    /// </summary>
    private static (double x, double y) FindFreeSlot(
        double startX, double startY,
        double w, double h,
        List<PlacedRect> placed,
        double maxX, double maxY)
    {
        double stepX = GridStep;
        double stepY = GridStep;

        double cy = startY;
        while (cy <= maxY - h + GridStep) // allow slight overshoot so we can clamp
        {
            // On the starting row honour the starting X; on subsequent rows start at 0
            double cx = (Math.Abs(cy - startY) < GridStep * 0.5) ? startX : 0.0;

            while (cx <= maxX - w + GridStep)
            {
                double clampedX = Math.Clamp(cx, 0.0, Math.Max(0.0, maxX - w));
                double clampedY = Math.Clamp(cy, 0.0, Math.Max(0.0, maxY - h));

                if (!HasOverlap(clampedX, clampedY, w, h, placed))
                    return (clampedX, clampedY);

                cx += stepX;
            }

            cy += stepY;
        }

        // Absolute fallback: stack below all placed fences
        double safeY = placed.Count > 0 ? placed.Max(p => p.Y + p.H) + GapFraction : 0.0;
        return (Snap(startX), Snap(Math.Min(safeY, Math.Max(0.0, maxY - h))));
    }

    private static bool HasOverlap(double x, double y, double w, double h, List<PlacedRect> placed)
    {
        foreach (var p in placed)
        {
            // Two rectangles overlap when they intersect (with a gap enforced)
            bool overlapX = x < p.X + p.W + GapFraction && x + w + GapFraction > p.X;
            bool overlapY = y < p.Y + p.H + GapFraction && y + h + GapFraction > p.Y;
            if (overlapX && overlapY)
                return true;
        }
        return false;
    }

    private static double Snap(double value) =>
        Math.Round(value / GridStep) * GridStep;
}
