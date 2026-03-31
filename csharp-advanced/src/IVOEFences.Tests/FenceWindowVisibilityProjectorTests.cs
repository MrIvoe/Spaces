using FluentAssertions;
using IVOEFences.Shell.Fences;
using Xunit;

namespace IVOEFences.Tests;

public class FenceWindowVisibilityProjectorTests
{
    [Fact]
    public void HiddenFence_IsNeverVisible()
    {
        bool visible = FenceWindowVisibilityProjector.ShouldBeVisible(
            baseVisible: true,
            isHidden: true,
            enableDesktopPages: false,
            currentPageIndex: 0,
            windowPageIndex: 0,
            isVisibleForActiveTab: true);

        visible.Should().BeFalse();
    }

    [Fact]
    public void PageMismatch_HidesFence_WhenDesktopPagesEnabled()
    {
        bool visible = FenceWindowVisibilityProjector.ShouldBeVisible(
            baseVisible: true,
            isHidden: false,
            enableDesktopPages: true,
            currentPageIndex: 1,
            windowPageIndex: 0,
            isVisibleForActiveTab: true);

        visible.Should().BeFalse();
    }

    [Fact]
    public void MatchingPageAndTab_KeepFenceVisible()
    {
        bool visible = FenceWindowVisibilityProjector.ShouldBeVisible(
            baseVisible: true,
            isHidden: false,
            enableDesktopPages: true,
            currentPageIndex: 2,
            windowPageIndex: 2,
            isVisibleForActiveTab: true);

        visible.Should().BeTrue();
    }

    [Fact]
    public void InactiveTab_HidesFence_EvenWhenBaseVisibilityIsOn()
    {
        bool visible = FenceWindowVisibilityProjector.ShouldBeVisible(
            baseVisible: true,
            isHidden: false,
            enableDesktopPages: false,
            currentPageIndex: 0,
            windowPageIndex: 0,
            isVisibleForActiveTab: false);

        visible.Should().BeFalse();
    }
}