using System.Collections.Concurrent;
using System.Drawing;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell.Fences;

internal sealed class AnimationManager
{
    // Key = (fence HWND, animation kind) — ensures only one animation per window per property
    private readonly ConcurrentDictionary<(IntPtr hwnd, string kind), CancellationTokenSource> _active = new();

    public async Task AnimateMove(FenceWindow fence, Point target, int durationMs = 300)
    {
        var cts = AcquireSlot(fence.Handle, "move");
        var ct = cts.Token;

        Point start = new(fence.X, fence.Y);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int x = (int)(start.X + (target.X - start.X) * t);
            int y = (int)(start.Y + (target.Y - start.Y) * t);
            fence.PostSetBounds(x, y, fence.Width, fence.Height);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            fence.PostSetBounds(target.X, target.Y, fence.Width, fence.Height);
    }

    public async Task AnimateResize(FenceWindow fence, Size targetSize, int durationMs = 300)
    {
        var cts = AcquireSlot(fence.Handle, "resize");
        var ct = cts.Token;

        Size start = new(fence.Width, fence.Height);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int w = (int)(start.Width + (targetSize.Width - start.Width) * t);
            int h = (int)(start.Height + (targetSize.Height - start.Height) * t);
            fence.PostSetBounds(fence.X, fence.Y, w, h);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            fence.PostSetBounds(fence.X, fence.Y, targetSize.Width, targetSize.Height);
    }

    public async Task AnimateOpacity(FenceWindow fence, int targetOpacityPercent, int durationMs = 200)
    {
        var cts = AcquireSlot(fence.Handle, "opacity");
        var ct = cts.Token;

        int start = fence.GetOpacityPercent();
        int target = Math.Clamp(targetOpacityPercent, 20, 100);
        int steps = Math.Max(1, durationMs / 15);

        AnimationService.Instance.Start(durationMs, 15);

        for (int i = 1; i <= steps; i++)
        {
            if (ct.IsCancellationRequested) return;
            float t = (float)i / steps;
            int value = (int)(start + (target - start) * t);
            fence.SetOpacityPercent(value);
            await Task.Delay(15, ct).ConfigureAwait(false);
        }

        if (!ct.IsCancellationRequested)
            fence.SetOpacityPercent(target);
    }

    /// <summary>Cancel any running animation for the given fence + kind, then register a new slot.</summary>
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
