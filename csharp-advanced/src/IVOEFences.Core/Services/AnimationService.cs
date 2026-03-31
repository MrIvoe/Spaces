namespace IVOEFences.Core.Services;

/// <summary>
/// Central animation scheduler decoupled from any specific UI backend.
/// Shell can subscribe and render with Win32/GDI/Direct2D.
/// </summary>
public sealed class AnimationService
{
    private static readonly Lazy<AnimationService> _instance = new(() => new AnimationService());
    public static AnimationService Instance => _instance.Value;

    public event EventHandler<AnimationTickEventArgs>? Tick;
    public event EventHandler<AnimationLifecycleEventArgs>? Started;
    public event EventHandler<AnimationLifecycleEventArgs>? Completed;

    private Timer? _timer;
    private DateTime _start;
    private int _durationMs;

    private AnimationService()
    {
    }

    public void Start(int durationMs = 220, int intervalMs = 16)
    {
        _durationMs = Math.Max(1, durationMs);
        _start = DateTime.UtcNow;
        _timer?.Dispose();
        _timer = new Timer(OnTick, null, 0, Math.Max(1, intervalMs));

        Started?.Invoke(this, new AnimationLifecycleEventArgs
        {
            DurationMs = _durationMs,
            StartedAtUtc = _start
        });
    }

    public void Stop()
    {
        _timer?.Dispose();
        _timer = null;
    }

    private void OnTick(object? _)
    {
        double elapsed = (DateTime.UtcNow - _start).TotalMilliseconds;
        double progress = Math.Clamp(elapsed / _durationMs, 0.0, 1.0);

        Tick?.Invoke(this, new AnimationTickEventArgs
        {
            Progress = progress,
            IsCompleted = progress >= 1.0
        });

        if (progress >= 1.0)
        {
            Completed?.Invoke(this, new AnimationLifecycleEventArgs
            {
                DurationMs = _durationMs,
                StartedAtUtc = _start
            });
            Stop();
        }
    }
}

public sealed class AnimationTickEventArgs : EventArgs
{
    public double Progress { get; init; }
    public bool IsCompleted { get; init; }
}

public sealed class AnimationLifecycleEventArgs : EventArgs
{
    public int DurationMs { get; init; }
    public DateTime StartedAtUtc { get; init; }
}
