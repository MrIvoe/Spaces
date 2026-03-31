namespace IVOEFences.Shell.Fences;

internal sealed record CreateFenceResult(
    Guid FenceId,
    bool WindowCreated,
    IntPtr WindowHandle);
