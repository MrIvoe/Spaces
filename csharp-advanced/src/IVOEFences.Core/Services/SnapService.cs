using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 30: Snap behavior service for magnetic fence alignment.
/// Handles snapping to screen edges, other fences, and monitor center.
/// </summary>
public sealed class SnapService
{
    private static SnapService? _instance;
    private static readonly object _lock = new();

    private readonly Models.AppSettings _settings;

    public static SnapService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new SnapService();
                }
            }
            return _instance;
        }
    }

    private SnapService()
    {
        _settings = new Models.AppSettings(); // Use default settings for now
    }

    /// <summary>
    /// Represents a snap result with adjusted position/size.
    /// </summary>
    public sealed class SnapResult
    {
        public double X { get; init; }
        public double Y { get; init; }
        public double Width { get; init; }
        public double Height { get; init; }
        public SnapType SnapType { get; init; }
        public bool Snapped { get; init; }
    }

    public enum SnapType
    {
        None = 0,
        ScreenEdgeLeft = 1,
        ScreenEdgeRight = 2,
        ScreenEdgeTop = 4,
        ScreenEdgeBottom = 8,
        MonitorCenter = 16,
        OtherFence = 32,
        Grid = 64
    }

    /// <summary>
    /// Snap fence position and size to nearest targets.
    /// Returns adjusted position/size and snap type applied.
    /// </summary>
    public SnapResult SnapPosition(
        double x, double y, double width, double height,
        System.Windows.Forms.Screen screen,
        Guid excludeFenceId)
    {
        if (screen == null)
            return new SnapResult { X = x, Y = y, Width = width, Height = height, Snapped = false };

        var workArea = screen.WorkingArea;
        double snappedX = x;
        double snappedY = y;
        double snappedWidth = width;
        double snappedHeight = height;
        var snapType = SnapType.None;
        bool snapped = false;

        int threshold = _settings.SnapThreshold;

        // Step 30: Snap to screen edges (horizontal)
        if (_settings.SnapToScreenEdges)
        {
            // Left edge
            double distLeft = Math.Abs(x - workArea.X);
            if (distLeft <= threshold && distLeft < 100) // Prevent snapping from far away
            {
                snappedX = workArea.X;
                snapType |= SnapType.ScreenEdgeLeft;
                snapped = true;
            }

            // Right edge
            double distRight = Math.Abs((x + width) - (workArea.X + workArea.Width));
            if (distRight <= threshold && distRight < 100)
            {
                snappedX = workArea.X + workArea.Width - width;
                snapType |= SnapType.ScreenEdgeRight;
                snapped = true;
            }

            // Top edge
            double distTop = Math.Abs(y - workArea.Y);
            if (distTop <= threshold && distTop < 100)
            {
                snappedY = workArea.Y;
                snapType |= SnapType.ScreenEdgeTop;
                snapped = true;
            }

            // Bottom edge
            double distBottom = Math.Abs((y + height) - (workArea.Y + workArea.Height));
            if (distBottom <= threshold && distBottom < 100)
            {
                snappedY = workArea.Y + workArea.Height - height;
                snapType |= SnapType.ScreenEdgeBottom;
                snapped = true;
            }
        }

        // Step 30: Snap to monitor center
        if (_settings.SnapToMonitorCenter)
        {
            double centerX = workArea.X + workArea.Width / 2.0;
            double centerY = workArea.Y + workArea.Height / 2.0;
            double fenceCenterX = snappedX + width / 2.0;
            double fenceCenterY = snappedY + height / 2.0;

            double distCenterX = Math.Abs(fenceCenterX - centerX);
            double distCenterY = Math.Abs(fenceCenterY - centerY);

            if (distCenterX <= threshold * 2)
            {
                snappedX = centerX - width / 2.0;
                snapType |= SnapType.MonitorCenter;
                snapped = true;
            }

            if (distCenterY <= threshold * 2)
            {
                snappedY = centerY - height / 2.0;
                snapType |= SnapType.MonitorCenter;
                snapped = true;
            }
        }

        // Step 30: Snap to other fences
        if (_settings.SnapToOtherFences)
        {
            var otherFences = GetOtherFencesOnScreen(screen, excludeFenceId);

            foreach (var other in otherFences)
            {
                // Horizontal snap
                // Left to other's right
                if (Math.Abs((snappedX) - (other.X + other.Width)) <= threshold)
                {
                    snappedX = other.X + other.Width;
                    snapType |= SnapType.OtherFence;
                    snapped = true;
                }
                // Right to other's left
                if (Math.Abs((snappedX + width) - other.X) <= threshold)
                {
                    snappedX = other.X - width;
                    snapType |= SnapType.OtherFence;
                    snapped = true;
                }

                // Vertical snap
                // Top to other's bottom
                if (Math.Abs((snappedY) - (other.Y + other.Height)) <= threshold)
                {
                    snappedY = other.Y + other.Height;
                    snapType |= SnapType.OtherFence;
                    snapped = true;
                }
                // Bottom to other's top
                if (Math.Abs((snappedY + height) - other.Y) <= threshold)
                {
                    snappedY = other.Y - height;
                    snapType |= SnapType.OtherFence;
                    snapped = true;
                }
            }
        }

        // Snap to grid — applied last so it refines the position
        if (_settings.GridSize > 0)
        {
            int g = _settings.GridSize;
            double gridX = Math.Round((snappedX - workArea.X) / g) * g + workArea.X;
            double gridY = Math.Round((snappedY - workArea.Y) / g) * g + workArea.Y;

            if (Math.Abs(gridX - snappedX) <= g / 2.0)
            {
                snappedX = gridX;
                snapType |= SnapType.Grid;
                snapped = true;
            }
            if (Math.Abs(gridY - snappedY) <= g / 2.0)
            {
                snappedY = gridY;
                snapType |= SnapType.Grid;
                snapped = true;
            }
        }

        return new SnapResult
        {
            X = snappedX,
            Y = snappedY,
            Width = snappedWidth,
            Height = snappedHeight,
            SnapType = snapType,
            Snapped = snapped
        };
    }

    /// <summary>
    /// Get list of other fences on the same screen for snap comparison.
    /// Returns fence bounds in screen coordinates.
    /// </summary>
    private List<(double X, double Y, double Width, double Height)> GetOtherFencesOnScreen(
        System.Windows.Forms.Screen screen, Guid excludeFenceId)
    {
        var result = new List<(double, double, double, double)>();

        try
        {
            var allFences = FenceStateService.Instance.Fences;
            var fencesOnScreen = allFences
                .Where(f => f.Id != excludeFenceId && f.MonitorDeviceName == screen.DeviceName)
                .ToList();

            foreach (var fence in fencesOnScreen)
            {
                var workArea = screen.WorkingArea;

                double x = workArea.X + workArea.Width * fence.XFraction;
                double y = workArea.Y + workArea.Height * fence.YFraction;
                double w = workArea.Width * fence.WidthFraction;
                double h = workArea.Height * fence.HeightFraction;

                result.Add((x, y, w, h));
            }
        }
        catch { }

        return result;
    }

    /// <summary>
    /// Check if snap is enabled globally.
    /// </summary>
    public bool IsSnapEnabled =>
        _settings.SnapToScreenEdges ||
        _settings.SnapToOtherFences ||
        _settings.SnapToMonitorCenter;
}
