using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Native;
using Xunit;

namespace IVOEFences.Tests;

public class FenceRestorePlacementPlannerTests
{
    [Fact]
    public void Plan_UsesResolvedMonitorArea_WhenNamedMonitorIsAvailable()
    {
        var planner = new FenceRestorePlacementPlanner((string deviceName, out Win32.RECT workArea) =>
        {
            deviceName.Should().Be("MONITOR-2");
            workArea = new Win32.RECT { left = 1000, top = 100, right = 2200, bottom = 900 };
            return true;
        });

        var model = new FenceModel
        {
            Title = "Resolved",
            MonitorDeviceName = "MONITOR-2",
            XFraction = 0.25,
            YFraction = 0.5,
            WidthFraction = 0.2,
            HeightFraction = 0.25,
        };

        FenceRestorePlacementPlanner.PlacementPlan plan = planner.Plan(model, new Win32.RECT { left = 0, top = 0, right = 500, bottom = 400 });

        plan.UsedFallbackWorkArea.Should().BeFalse();
        plan.TargetArea.left.Should().Be(1000);
        plan.TargetArea.top.Should().Be(100);
        plan.Width.Should().Be(240);
        plan.Height.Should().Be(200);
        plan.X.Should().Be(1300);
        plan.Y.Should().Be(500);
    }

    [Fact]
    public void Plan_FallsBackAndClamps_WhenStoredMonitorIsUnavailable()
    {
        var planner = new FenceRestorePlacementPlanner((string _, out Win32.RECT workArea) =>
        {
            workArea = default;
            return false;
        });

        var model = new FenceModel
        {
            Title = "Fallback",
            MonitorDeviceName = "MISSING",
            XFraction = 0.95,
            YFraction = 0.9,
            WidthFraction = 0.4,
            HeightFraction = 0.5,
        };

        FenceRestorePlacementPlanner.PlacementPlan plan = planner.Plan(model, new Win32.RECT { left = 0, top = 0, right = 1000, bottom = 800 });

        plan.UsedFallbackWorkArea.Should().BeTrue();
        plan.Width.Should().Be(400);
        plan.Height.Should().Be(400);
        plan.X.Should().Be(600);
        plan.Y.Should().Be(400);
    }

    [Fact]
    public void Plan_ComputesDockedBarBounds_FromTargetArea()
    {
        var planner = new FenceRestorePlacementPlanner((string _, out Win32.RECT workArea) =>
        {
            workArea = new Win32.RECT { left = 50, top = 60, right = 850, bottom = 660 };
            return true;
        });

        var model = new FenceModel
        {
            Title = "Docked",
            MonitorDeviceName = "MONITOR-LEFT",
            IsBar = true,
            DockEdge = DockEdge.Left,
            BarThickness = 120,
        };

        FenceRestorePlacementPlanner.PlacementPlan plan = planner.Plan(model, new Win32.RECT { left = 0, top = 0, right = 100, bottom = 100 });

        plan.UsedFallbackWorkArea.Should().BeFalse();
        plan.X.Should().Be(50);
        plan.Y.Should().Be(60);
        plan.Width.Should().Be(120);
        plan.Height.Should().Be(600);
    }
}