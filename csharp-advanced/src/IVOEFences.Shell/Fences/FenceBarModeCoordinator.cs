using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Desktop;
using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceBarModeCoordinator
{
    private static readonly int[] BarThicknessOptions = { 40, 56, 72, 88, 104, 128, 160, 192 };

    public bool ToggleBarMode(FenceWindow window, FenceModel model, Action onDisableBar)
    {
        if (!model.IsBar || model.DockEdge == DockEdge.None)
        {
            model.IsBar = true;
            model.DockEdge = model.DockEdge == DockEdge.None ? DockEdge.Top : model.DockEdge;
            model.BarThickness = Math.Clamp(model.BarThickness, 30, 220);
            ApplyBarLayout(window, model);
        }
        else
        {
            model.IsBar = false;
            model.DockEdge = DockEdge.None;
            onDisableBar();
        }

        window.InvalidateContent();
        FenceStateService.Instance.MarkDirty();
        return true;
    }

    public bool CycleBarDockEdge(FenceWindow window, FenceModel model)
    {
        if (!model.IsBar)
            return false;

        model.DockEdge = model.DockEdge switch
        {
            DockEdge.Top => DockEdge.Right,
            DockEdge.Right => DockEdge.Bottom,
            DockEdge.Bottom => DockEdge.Left,
            DockEdge.Left => DockEdge.Top,
            _ => DockEdge.Top,
        };

        ApplyBarLayout(window, model);
        FenceStateService.Instance.MarkDirty();
        return true;
    }

    public bool CycleBarThickness(FenceWindow window, FenceModel model)
    {
        if (!model.IsBar)
            return false;

        model.BarThickness = NextInt(model.BarThickness, BarThicknessOptions);
        ApplyBarLayout(window, model);
        FenceStateService.Instance.MarkDirty();
        return true;
    }

    public string GetBarSummary(FenceModel model)
    {
        if (!model.IsBar || model.DockEdge == DockEdge.None)
            return "Off";

        int thickness = Math.Clamp(model.BarThickness, 30, 220);
        return $"{model.DockEdge} ({thickness}px)";
    }

    private static void ApplyBarLayout(FenceWindow window, FenceModel model)
    {
        IntPtr monitor = Win32.MonitorFromWindow(window.Handle, Win32.MONITOR_DEFAULTTONEAREST);
        var info = new Win32.MONITORINFOEX { cbSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<Win32.MONITORINFOEX>() };

        Win32.RECT wa = DesktopHost.GetPrimaryWorkArea();
        if (monitor != IntPtr.Zero && Win32.GetMonitorInfo(monitor, ref info))
            wa = info.rcWork;

        (int x, int y, int w, int h) = ComputeBarBounds(wa, model.DockEdge, model.BarThickness);
        window.SetBounds(x, y, w, h);

        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);

        model.XFraction = (double)(x - wa.left) / waW;
        model.YFraction = (double)(y - wa.top) / waH;
        model.WidthFraction = (double)w / waW;
        model.HeightFraction = (double)h / waH;
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

    private static int NextInt(int current, IReadOnlyList<int> values)
    {
        for (int i = 0; i < values.Count; i++)
        {
            if (values[i] > current)
                return values[i];
        }

        return values[0];
    }
}
