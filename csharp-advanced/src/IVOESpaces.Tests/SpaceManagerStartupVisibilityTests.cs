using FluentAssertions;
using IVOESpaces.Core.Models;
using IVOESpaces.Shell.Spaces;
using Xunit;

namespace IVOESpaces.Tests;

public class SpaceManagerStartupVisibilityTests
{
    [Fact]
    public void ShouldShowWindowOnCreate_HiddenSpace_ReturnsFalse()
    {
        var model = new SpaceModel
        {
            IsHidden = true,
            WidthFraction = 0.2,
            HeightFraction = 0.2,
        };

        bool result = SpaceManager.ShouldShowWindowOnCreate(
            model,
            enableDesktopPages: false,
            currentPageIndex: 0,
            activeTabs: new Dictionary<Guid, int>());

        result.Should().BeFalse();
    }

    [Fact]
    public void ShouldShowWindowOnCreate_InvalidBounds_ReturnsFalse()
    {
        var model = new SpaceModel
        {
            IsHidden = false,
            WidthFraction = 0,
            HeightFraction = 0.2,
        };

        bool result = SpaceManager.ShouldShowWindowOnCreate(
            model,
            enableDesktopPages: false,
            currentPageIndex: 0,
            activeTabs: new Dictionary<Guid, int>());

        result.Should().BeFalse();
    }

    [Fact]
    public void ShouldShowWindowOnCreate_PageMismatch_ReturnsFalse()
    {
        var model = new SpaceModel
        {
            IsHidden = false,
            PageIndex = 1,
            WidthFraction = 0.2,
            HeightFraction = 0.2,
        };

        bool result = SpaceManager.ShouldShowWindowOnCreate(
            model,
            enableDesktopPages: true,
            currentPageIndex: 0,
            activeTabs: new Dictionary<Guid, int>());

        result.Should().BeFalse();
    }

    [Fact]
    public void ShouldShowWindowOnCreate_InactiveTab_ReturnsFalse()
    {
        Guid tabContainerId = Guid.NewGuid();
        var model = new SpaceModel
        {
            IsHidden = false,
            TabContainerId = tabContainerId,
            TabIndex = 1,
            WidthFraction = 0.2,
            HeightFraction = 0.2,
        };

        bool result = SpaceManager.ShouldShowWindowOnCreate(
            model,
            enableDesktopPages: false,
            currentPageIndex: 0,
            activeTabs: new Dictionary<Guid, int> { [tabContainerId] = 0 });

        result.Should().BeFalse();
    }

    [Fact]
    public void ShouldShowWindowOnCreate_ProjectedVisible_ReturnsTrue()
    {
        var model = new SpaceModel
        {
            IsHidden = false,
            PageIndex = 0,
            WidthFraction = 0.2,
            HeightFraction = 0.2,
        };

        bool result = SpaceManager.ShouldShowWindowOnCreate(
            model,
            enableDesktopPages: true,
            currentPageIndex: 0,
            activeTabs: new Dictionary<Guid, int>());

        result.Should().BeTrue();
    }
}