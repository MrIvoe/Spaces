using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;
using IVOEFences.Shell;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 39: Service for managing quick-launch bar mode docking.
/// Handles positioning fences as screen-edge bars, taskbar avoidance, and layout.
/// </summary>
public sealed class BarModeService
{
    private static BarModeService? _instance;
    private static readonly object _lock = new();

    private readonly MonitorService _monitorService;

    public static BarModeService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new BarModeService();
                }
            }
            return _instance;
        }
    }

    private BarModeService()
    {
        _monitorService = MonitorService.Instance;
    }

    /// <summary>
    /// Step 39: Structure representing bar positioning information.
    /// </summary>
    public sealed record BarPositionInfo
    {
        public int X { get; init; }
        public int Y { get; init; }
        public int Width { get; init; }
        public int Height { get; init; }
        public bool IsVertical { get; init; } // true if Left/Right edge
    }

    /// <summary>
    /// Step 39: Calculate the correct position and dimensions for a bar fence.
    /// Returns positioning information based on DockEdge and monitor layout.
    /// </summary>
    public BarPositionInfo CalculateBarPosition(FenceModel fence)
    {
        if (fence.DockEdge == DockEdge.None)
        {
            throw new ArgumentException("Fence is not docked to an edge", nameof(fence));
        }

        try
        {
            // Get the monitor where this bar should appear
            var monitor = GetMonitorForBar(fence);
            if (monitor == null)
            {
                throw new InvalidOperationException($"Monitor not found: {fence.MonitorDeviceName}");
            }

            var workArea = monitor.WorkingArea; // System.Drawing.Rectangle
            var taskbarInfo = GetTaskbarInfo();
            
            // Calculate position based on dock edge
            return fence.DockEdge switch
            {
                DockEdge.Top => CalculateTopPosition(workArea, taskbarInfo, fence.BarThickness),
                DockEdge.Bottom => CalculateBottomPosition(workArea, taskbarInfo, fence.BarThickness),
                DockEdge.Left => CalculateLeftPosition(workArea, taskbarInfo, fence.BarThickness),
                DockEdge.Right => CalculateRightPosition(workArea, taskbarInfo, fence.BarThickness),
                _ => throw new ArgumentException("Invalid DockEdge value")
            };
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to calculate bar position for fence {FenceId}", fence.Id);
            // Return safe default position (top-left corner)
            return new BarPositionInfo { X = 0, Y = 0, Width = 56, Height = 56, IsVertical = false };
        }
    }

    /// <summary>
    /// Step 39: Get the monitor where this bar should be displayed.
    /// </summary>
    private System.Windows.Forms.Screen? GetMonitorForBar(FenceModel fence)
    {
        if (string.IsNullOrEmpty(fence.MonitorDeviceName))
            return System.Windows.Forms.Screen.PrimaryScreen;

        return System.Windows.Forms.Screen.AllScreens
            .FirstOrDefault(s => s.DeviceName == fence.MonitorDeviceName)
            ?? System.Windows.Forms.Screen.PrimaryScreen;
    }

    /// <summary>
    /// Step 39: Get taskbar information for avoidance calculations.
    /// </summary>
    private TaskbarInfo GetTaskbarInfo()
    {
        // Get Windows taskbar position and size
        var taskbarEdge = DetectTaskbarEdge();
        var taskbarSize = DetectTaskbarSize();

        return new TaskbarInfo
        {
            Edge = taskbarEdge,
            ThicknessPixels = taskbarSize
        };
    }

    private sealed record TaskbarInfo
    {
        public DockEdge Edge { get; init; } = DockEdge.Bottom;
        public int ThicknessPixels { get; init; } = 40;
    }

    /// <summary>
    /// Step 39: Detect which screen edge the Windows taskbar is on (usually Bottom).
    /// </summary>
    private DockEdge DetectTaskbarEdge()
    {
        try
        {
            // Windows taskbar is typically at the bottom
            // This is a simplified implementation; full implementation would use Win32 APIs
            return DockEdge.Bottom;
        }
        catch
        {
            return DockEdge.Bottom; // Safe default
        }
    }

    /// <summary>
    /// Step 39: Detect the thickness of the Windows taskbar in pixels.
    /// </summary>
    private int DetectTaskbarSize()
    {
        try
        {
            // Standard Windows taskbar height is ~40 pixels
            // Full implementation would use Win32 ShellHook or similar
            return 40;
        }
        catch
        {
            return 40; // Safe default
        }
    }

    private BarPositionInfo CalculateTopPosition(System.Drawing.Rectangle workArea, TaskbarInfo taskbar, int barThickness)
    {
        int x = workArea.X;
        int y = workArea.Y;
        int width = workArea.Width;
        int height = barThickness;

        // If taskbar is on top, avoid it
        if (taskbar.Edge == DockEdge.Top)
            y += taskbar.ThicknessPixels;

        return new BarPositionInfo
        {
            X = x,
            Y = y,
            Width = width,
            Height = height,
            IsVertical = false
        };
    }

    private BarPositionInfo CalculateBottomPosition(System.Drawing.Rectangle workArea, TaskbarInfo taskbar, int barThickness)
    {
        int x = workArea.X;
        int y = workArea.Y + workArea.Height - barThickness;
        int width = workArea.Width;
        int height = barThickness;

        // If taskbar is on bottom, move bar above it
        if (taskbar.Edge == DockEdge.Bottom)
            y -= taskbar.ThicknessPixels;

        return new BarPositionInfo
        {
            X = x,
            Y = y,
            Width = width,
            Height = height,
            IsVertical = false
        };
    }

    private BarPositionInfo CalculateLeftPosition(System.Drawing.Rectangle workArea, TaskbarInfo taskbar, int barThickness)
    {
        int x = workArea.X;
        int y = workArea.Y;
        int width = barThickness;
        int height = workArea.Height;

        // If taskbar is on left, avoid it
        if (taskbar.Edge == DockEdge.Left)
            x += taskbar.ThicknessPixels;

        return new BarPositionInfo
        {
            X = x,
            Y = y,
            Width = width,
            Height = height,
            IsVertical = true
        };
    }

    private BarPositionInfo CalculateRightPosition(System.Drawing.Rectangle workArea, TaskbarInfo taskbar, int barThickness)
    {
        int x = workArea.X + workArea.Width - barThickness;
        int y = workArea.Y;
        int width = barThickness;
        int height = workArea.Height;

        // If taskbar is on right, move bar to the left of it
        if (taskbar.Edge == DockEdge.Right)
            x -= taskbar.ThicknessPixels;

        return new BarPositionInfo
        {
            X = x,
            Y = y,
            Width = width,
            Height = height,
            IsVertical = true
        };
    }

    /// <summary>
    /// Step 39: Enable bar mode for a fence.
    /// Sets up docking and positioning.
    /// </summary>
    public bool EnableBarMode(FenceModel fence, DockEdge edge, int thickness)
    {
        if (fence == null)
            return false;

        try
        {
            fence.IsBar = true;
            fence.DockEdge = edge;
            fence.BarThickness = Math.Max(30, Math.Min(thickness, 200)); // Clamp to 30-200 pixels

            var posInfo = CalculateBarPosition(fence);

            // Convert pixel coordinates to fractions
            var monitor = GetMonitorForBar(fence);
            if (monitor != null)
            {
                var workArea = monitor.WorkingArea;
                fence.XFraction = (posInfo.X - workArea.X) / (double)workArea.Width;
                fence.YFraction = (posInfo.Y - workArea.Y) / (double)workArea.Height;
                fence.WidthFraction = posInfo.Width / (double)workArea.Width;
                fence.HeightFraction = posInfo.Height / (double)workArea.Height;
            }

            Serilog.Log.Information("Bar mode enabled for fence {FenceId}: {Edge} edge, {Thickness}px",
                fence.Id, edge, thickness);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to enable bar mode for fence {FenceId}", fence.Id);
            return false;
        }
    }

    /// <summary>
    /// Step 39: Disable bar mode and revert to standard fence.
    /// </summary>
    public bool DisableBarMode(FenceModel fence)
    {
        if (fence == null)
            return false;

        try
        {
            fence.IsBar = false;
            fence.DockEdge = DockEdge.None;

            // Position to default location (center-ish)
            fence.XFraction = 0.2;
            fence.YFraction = 0.2;
            fence.WidthFraction = 0.3;
            fence.HeightFraction = 0.4;

            Serilog.Log.Information("Bar mode disabled for fence {FenceId}", fence.Id);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to disable bar mode for fence {FenceId}", fence.Id);
            return false;
        }
    }

    /// <summary>
    /// Step 39: Update bar thickness and recalculate position.
    /// </summary>
    public bool UpdateBarThickness(FenceModel fence, int newThickness)
    {
        if (fence == null || fence.DockEdge == DockEdge.None)
            return false;

        try
        {
            fence.BarThickness = Math.Max(30, Math.Min(newThickness, 200));
            
            var posInfo = CalculateBarPosition(fence);
            var monitor = GetMonitorForBar(fence);
            
            if (monitor != null)
            {
                var workArea = monitor.WorkingArea;
                fence.WidthFraction = posInfo.Width / (double)workArea.Width;
                fence.HeightFraction = posInfo.Height / (double)workArea.Height;
            }

            Serilog.Log.Debug("Bar thickness updated: {FenceId} = {Thickness}px", fence.Id, newThickness);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to update bar thickness for fence {FenceId}", fence.Id);
            return false;
        }
    }

    /// <summary>
    /// Step 39: Change the dock edge for an existing bar.
    /// </summary>
    public bool ChangeBarEdge(FenceModel fence, DockEdge newEdge)
    {
        if (fence == null || !fence.IsBar)
            return false;

        try
        {
            fence.DockEdge = newEdge;
            
            var posInfo = CalculateBarPosition(fence);
            var monitor = GetMonitorForBar(fence);
            
            if (monitor != null)
            {
                var workArea = monitor.WorkingArea;
                fence.XFraction = (posInfo.X - workArea.X) / (double)workArea.Width;
                fence.YFraction = (posInfo.Y - workArea.Y) / (double)workArea.Height;
                fence.WidthFraction = posInfo.Width / (double)workArea.Width;
                fence.HeightFraction = posInfo.Height / (double)workArea.Height;
            }

            Serilog.Log.Information("Bar edge changed: {FenceId} = {Edge}", fence.Id, newEdge);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to change bar edge for fence {FenceId}", fence.Id);
            return false;
        }
    }

    /// <summary>
    /// Step 39: Get all bars that are currently active.
    /// </summary>
    public List<FenceModel> GetAllBars()
    {
        try
        {
            return FenceStateService.Instance.Fences
                .Where(f => f.IsBar && f.DockEdge != DockEdge.None)
                .ToList();
        }
        catch
        {
            return new List<FenceModel>();
        }
    }

    /// <summary>
    /// Step 39: Check if there's already a bar on the given edge for the given monitor.
    /// </summary>
    public bool IsEdgeOccupied(string monitorDeviceName, DockEdge edge)
    {
        try
        {
            var bars = GetAllBars();
            return bars.Any(b => b.MonitorDeviceName == monitorDeviceName && b.DockEdge == edge);
        }
        catch
        {
            return false;
        }
    }
}
