namespace IVOESpaces.Shell.Spaces;

internal static class SpaceWindowVisibilityProjector
{
    public static bool ShouldBeVisible(
        bool baseVisible,
        bool isHidden,
        bool enableDesktopPages,
        int currentPageIndex,
        int windowPageIndex,
        bool isVisibleForActiveTab)
    {
        if (!baseVisible || isHidden || !isVisibleForActiveTab)
            return false;

        return !enableDesktopPages || windowPageIndex == currentPageIndex;
    }
}