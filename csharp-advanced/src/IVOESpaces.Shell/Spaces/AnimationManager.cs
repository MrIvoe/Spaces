using System.Collections.Concurrent;
using System.Drawing;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell.Spaces;

internal sealed class AnimationManager
{
    // Key = (space HWND, animation kind) — ensures only one animation per window per property
    private readonly ConcurrentDictionary<(IntPtr hwnd, string kind), CancellationTokenSource> _active = new();

    public async Task AnimateMove(SpaceWindow space, Point target, int durationMs = 300)
    {
        var cts = AcquireSlot(space.Handle, "move");
        var ct = cts.Token;

        Point start = new(space.X, space.Y);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int x = (int)(start.X + (target.X - start.X) * t);
            int y = (int)(start.Y + (target.Y - start.Y) * t);
            space.PostSetBounds(x, y, space.Width, space.Height);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            space.PostSetBounds(target.X, target.Y, space.Width, space.Height);
    }

    public async Task AnimateResize(SpaceWindow space, Size targetSize, int durationMs = 300)
    {
        var cts = AcquireSlot(space.Handle, "resize");
        var ct = cts.Token;

        Size start = new(space.Width, space.Height);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int w = (int)(start.Width + (targetSize.Width - start.Width) * t);
            int h = (int)(start.Height + (targetSize.Height - start.Height) * t);
            space.PostSetBounds(space.X, space.Y, w, h);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            space.PostSetBounds(space.X, space.Y, targetSize.Width, targetSize.Height);
    }

    public async Task AnimateOpacity(SpaceWindow space, int targetOpacityPercent, int durationMs = 200)
    {
        var cts = AcquireSlot(space.Handle, "opacity");
        var ct = cts.Token;

        int start = space.GetOpacityPercent();
        int target = Math.Clamp(targetOpacityPercent, 20, 100);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int value = (int)(start + (target - start) * t);
            space.SetOpacityPercent(value);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            space.SetOpacityPercent(target);
    }

    /// <summary>Cancel any running animation for the given space + kind, then register a new slot.</summary>
    private CancellationTokenSource AcquireSlot(IntPtr hwnd, string kind)
    {
        var key = (hwnd, kind);
        var newCts = new CancellationTokenSource();

        if (_active.TryGetValue(key, out var oldCts))
        {
            oldCts.Cancel();
            oldCts.Dispose();
        }

        _active[key] = newCts;
        return newCts;
    }
}
