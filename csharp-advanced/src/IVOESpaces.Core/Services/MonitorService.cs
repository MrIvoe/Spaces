using IVOESpaces.Core.Models;
using System.Drawing;
using System.Windows.Forms;

namespace IVOESpaces.Core.Services;

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

    public PointF FractionToPoint(SpaceModel space, MonitorInfo monitor)
    {
        return new PointF(
            (float)(space.XFraction * monitor.WorkArea.Width + monitor.WorkArea.X),
            (float)(space.YFraction * monitor.WorkArea.Height + monitor.WorkArea.Y));
    }

    public SizeF FractionToSize(SpaceModel space, MonitorInfo monitor)
    {
        return new SizeF(
            (float)(space.WidthFraction * monitor.WorkArea.Width),
            (float)(space.HeightFraction * monitor.WorkArea.Height));
    }

    public void UpdateSpaceForDisconnectedMonitor(SpaceModel space)
    {
        var monitors = GetMonitors();
        if (monitors.All(m => m.DeviceName != space.MonitorDeviceName))
        {
            var primary = monitors.FirstOrDefault(m => m.DeviceName == System.Windows.Forms.Screen.PrimaryScreen.DeviceName)
                          ?? monitors.FirstOrDefault();
            if (primary is not null)
                space.MonitorDeviceName = primary.DeviceName;
        }
    }

    /// <summary>Convert a space's stored fractions to a pixel rect on the given monitor.</summary>
    public System.Windows.Rect FractionToPixels(SpaceModel space, MonitorInfo monitor)
    {
        return new System.Windows.Rect(
            monitor.WorkArea.X + space.XFraction * monitor.WorkArea.Width,
            monitor.WorkArea.Y + space.YFraction * monitor.WorkArea.Height,
            space.WidthFraction * monitor.WorkArea.Width,
            space.HeightFraction * monitor.WorkArea.Height);
    }

    /// <summary>Update a space's fraction fields from a pixel rect on the given monitor.</summary>
    public void UpdateFractions(SpaceModel space, System.Windows.Rect pixelRect, MonitorInfo monitor)
    {
        if (monitor.WorkArea.Width <= 0 || monitor.WorkArea.Height <= 0) return;
        space.XFraction = (pixelRect.X - monitor.WorkArea.X) / monitor.WorkArea.Width;
        space.YFraction = (pixelRect.Y - monitor.WorkArea.Y) / monitor.WorkArea.Height;
        space.WidthFraction = pixelRect.Width / monitor.WorkArea.Width;
        space.HeightFraction = pixelRect.Height / monitor.WorkArea.Height;
        space.MonitorDeviceName = monitor.DeviceName;
    }

    /// <summary>
    /// After a display change, move any space whose saved monitor is no longer connected
    /// to a safe position on the primary monitor.
    /// </summary>
    public void RelocateOrphanedSpaces(IEnumerable<SpaceModel> spaces)
    {
        var currentMonitors = GetMonitors();
        var primary = currentMonitors.FirstOrDefault(m =>
                          m.DeviceName == System.Windows.Forms.Screen.PrimaryScreen?.DeviceName)
                      ?? currentMonitors.FirstOrDefault();
        if (primary is null) return;

        foreach (var space in spaces)
        {
            bool known = currentMonitors.Any(m => m.DeviceName == space.MonitorDeviceName);
            if (!known)
            {
                space.MonitorDeviceName = primary.DeviceName;
                space.XFraction = 0.05;
                space.YFraction = 0.05;
            }
        }
    }
}
