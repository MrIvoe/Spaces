using IVOEFences.Core.Models;
using IVOEFences.Shell.Native;
using System.Runtime.InteropServices;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceRestorePlacementPlanner
{
    internal delegate bool TryResolveMonitorWorkArea(string deviceName, out Win32.RECT workArea);

    private readonly TryResolveMonitorWorkArea _workAreaResolver;

    public FenceRestorePlacementPlanner(TryResolveMonitorWorkArea? workAreaResolver = null)
    {
        _workAreaResolver = workAreaResolver ?? TryGetWorkAreaByDeviceNameFromSystem;
    }

    internal PlacementPlan Plan(FenceModel model, Win32.RECT fallbackWorkArea)
    {
        Win32.RECT targetArea = fallbackWorkArea;
        bool usedFallback = true;

        if (!string.IsNullOrWhiteSpace(model.MonitorDeviceName)
            && _workAreaResolver(model.MonitorDeviceName, out Win32.RECT resolvedArea)
            && resolvedArea.right > resolvedArea.left
            && resolvedArea.bottom > resolvedArea.top)
        {
            targetArea = resolvedArea;
            usedFallback = false;
        }

        if (targetArea.right <= targetArea.left || targetArea.bottom <= targetArea.top)
        {
            targetArea = fallbackWorkArea;
            usedFallback = true;
        }

        int targetW = Math.Max(targetArea.right - targetArea.left, 1);
        int targetH = Math.Max(targetArea.bottom - targetArea.top, 1);

        double xFraction = NormalizeFraction(model.XFraction, 0.05);
        double yFraction = NormalizeFraction(model.YFraction, 0.05);
        double widthFraction = NormalizeFraction(model.WidthFraction, 0.22, minimum: 0.05);
        double heightFraction = NormalizeFraction(model.HeightFraction, 0.28, minimum: 0.05);

        int x = (int)(xFraction * targetW) + targetArea.left;
        int y = (int)(yFraction * targetH) + targetArea.top;
        int width = Math.Max((int)(widthFraction * targetW), 80);
        int height = Math.Max((int)(heightFraction * targetH), 60);

        if (model.IsBar && model.DockEdge != DockEdge.None)
            (x, y, width, height) = ComputeBarBounds(targetArea, model.DockEdge, model.BarThickness);

        x = Math.Clamp(x, targetArea.left, Math.Max(targetArea.left, targetArea.right - width));
        y = Math.Clamp(y, targetArea.top, Math.Max(targetArea.top, targetArea.bottom - height));

        return new PlacementPlan(targetArea, x, y, width, height, usedFallback);
    }

    internal readonly record struct PlacementPlan(
        Win32.RECT TargetArea,
        int X,
        int Y,
        int Width,
        int Height,
        bool UsedFallbackWorkArea);

    internal static bool TryGetWorkAreaByDeviceNameFromSystem(string deviceName, out Win32.RECT workArea)
    {
        Win32.RECT resolvedArea = default;
        bool found = false;

        bool Callback(IntPtr hMonitor, IntPtr _hdc, ref Win32.RECT _monitorRect, IntPtr _data)
        {
            var mi = new Win32.MONITORINFOEX { cbSize = (uint)Marshal.SizeOf<Win32.MONITORINFOEX>() };
            if (!Win32.GetMonitorInfo(hMonitor, ref mi))
                return true;

            if (!string.Equals(mi.szDevice, deviceName, StringComparison.OrdinalIgnoreCase))
                return true;

            resolvedArea = mi.rcWork;
            found = true;
            return false;
        }

        Win32.EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, Callback, IntPtr.Zero);
        workArea = resolvedArea;
        return found;
    }

    private static double NormalizeFraction(double value, double fallback, double minimum = 0)
    {
        if (double.IsNaN(value) || double.IsInfinity(value))
            return fallback;

        return Math.Clamp(value, Math.Max(0, minimum), 1);
    }

    private static (int x, int y, int w, int h) ComputeBarBounds(Win32.RECT wa, DockEdge edge, int thickness)
    {
        int clampedThickness = Math.Clamp(thickness, 30, 220);
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);

        return edge switch
        {
            DockEdge.Top => (wa.left, wa.top, waW, Math.Min(clampedThickness, waH)),
            DockEdge.Bottom => (wa.left, wa.bottom - Math.Min(clampedThickness, waH), waW, Math.Min(clampedThickness, waH)),
            DockEdge.Left => (wa.left, wa.top, Math.Min(clampedThickness, waW), waH),
            DockEdge.Right => (wa.right - Math.Min(clampedThickness, waW), wa.top, Math.Min(clampedThickness, waW), waH),
            _ => (wa.left, wa.top, waW, Math.Min(clampedThickness, waH)),
        };
    }
}