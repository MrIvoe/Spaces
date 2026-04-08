namespace IVOESpaces.Shell.Spaces;

internal sealed record CreateSpaceResult(
    Guid SpaceId,
    bool WindowCreated,
    IntPtr WindowHandle);
