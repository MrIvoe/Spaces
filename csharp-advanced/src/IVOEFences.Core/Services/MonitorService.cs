using IVOEFences.Core.Models;
using System.Drawing;
using System.Windows.Forms;

namespace IVOEFences.Core.Services;

public class MonitorService
{
    private static readonly Lazy<MonitorService> _instance = new(() => new MonitorService());
    public static MonitorService Instance => _instance.Value;

    public record MonitorInfo(string DeviceName, Rectangle WorkArea, Rectangle Bounds, float DpiScale);

    public IReadOnlyList<MonitorInfo> GetMonitors()
    {
        return Screen.AllScreens.Select(s => new MonitorInfo(
            s.DeviceName,
            s.WorkingArea,
            s.Bounds,
            GetDpiScaleForScreen(s))).ToList();
    }

    private float GetDpiScaleForScreen(System.Windows.Forms.Screen screen)
    {
        // Placeholder: default 1.0 (96 DPI). For accurate DPI, P/Invoke GetDpiForMonitor in shell project.
        return 1.0f;
    }

    public PointF FractionToPoint(FenceModel fence, MonitorInfo monitor)
    {
        return new PointF(
            (float)(fence.XFraction * monitor.WorkArea.Width + monitor.WorkArea.X),
            (float)(fence.YFraction * monitor.WorkArea.Height + monitor.WorkArea.Y));
    }

    public SizeF FractionToSize(FenceModel fence, MonitorInfo monitor)
    {
        return new SizeF(
            (float)(fence.WidthFraction * monitor.WorkArea.Width),
            (float)(fence.HeightFraction * monitor.WorkArea.Height));
    }

    public void UpdateFenceForDisconnectedMonitor(FenceModel fence)
    {
        var monitors = GetMonitors();
        if (monitors.All(m => m.DeviceName != fence.MonitorDeviceName))
        {
            var primary = monitors.FirstOrDefault(m => m.DeviceName == System.Windows.Forms.Screen.PrimaryScreen.DeviceName)
                          ?? monitors.FirstOrDefault();
            if (primary is not null)
                fence.MonitorDeviceName = primary.DeviceName;
        }
    }

    /// <summary>Convert a fence's stored fractions to a pixel rect on the given monitor.</summary>
    public System.Windows.Rect FractionToPixels(FenceModel fence, MonitorInfo monitor)
    {
        return new System.Windows.Rect(
            monitor.WorkArea.X + fence.XFraction * monitor.WorkArea.Width,
            monitor.WorkArea.Y + fence.YFraction * monitor.WorkArea.Height,
            fence.WidthFraction * monitor.WorkArea.Width,
            fence.HeightFraction * monitor.WorkArea.Height);
    }

    /// <summary>Update a fence's fraction fields from a pixel rect on the given monitor.</summary>
    public void UpdateFractions(FenceModel fence, System.Windows.Rect pixelRect, MonitorInfo monitor)
    {
        if (monitor.WorkArea.Width <= 0 || monitor.WorkArea.Height <= 0) return;
        fence.XFraction = (pixelRect.X - monitor.WorkArea.X) / monitor.WorkArea.Width;
        fence.YFraction = (pixelRect.Y - monitor.WorkArea.Y) / monitor.WorkArea.Height;
        fence.WidthFraction = pixelRect.Width / monitor.WorkArea.Width;
        fence.HeightFraction = pixelRect.Height / monitor.WorkArea.Height;
        fence.MonitorDeviceName = monitor.DeviceName;
    }

    /// <summary>
    /// After a display change, move any fence whose saved monitor is no longer connected
    /// to a safe position on the primary monitor.
    /// </summary>
    public void RelocateOrphanedFences(IEnumerable<FenceModel> fences)
    {
        var currentMonitors = GetMonitors();
        var primary = currentMonitors.FirstOrDefault(m =>
                          m.DeviceName == System.Windows.Forms.Screen.PrimaryScreen?.DeviceName)
                      ?? currentMonitors.FirstOrDefault();
        if (primary is null) return;

        foreach (var fence in fences)
        {
            bool known = currentMonitors.Any(m => m.DeviceName == fence.MonitorDeviceName);
            if (!known)
            {
                fence.MonitorDeviceName = primary.DeviceName;
                fence.XFraction = 0.05;
                fence.YFraction = 0.05;
            }
        }
    }
}
