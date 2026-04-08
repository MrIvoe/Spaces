using IVOESpaces.Shell.Native;
using Serilog;
using System;
using System.Threading;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Detects mouse inactivity (idle) and notifies subscribers so spaces can be
/// faded out during idle periods, restoring them on the next mouse movement.
///
/// Polls the cursor position every <see cref="PollIntervalMs"/> milliseconds on
/// a background thread. When the cursor has not moved for
/// <see cref="IdleThresholdSeconds"/> seconds, fires <see cref="IdleStarted"/>.
/// The next detected movement fires <see cref="IdleEnded"/>.
/// </summary>
internal sealed class IdleModeService : IDisposable
{
    public const int PollIntervalMs    = 500;    // check every 0.5 s for responsive idle enter/exit
    public const int IdleThresholdSeconds = 300; // 5 minutes default

    private Win32.POINT _lastCursorPos;
    private DateTime    _lastMoveTime = DateTime.UtcNow;
    private bool        _idleActive;
    private volatile bool _disposed;
    private int         _thresholdSeconds;
    private readonly Timer _timer;

    public event EventHandler? IdleStarted;
    public event EventHandler? IdleEnded;

    public bool IsIdle => _idleActive;

    public IdleModeService(int thresholdSeconds = IdleThresholdSeconds)
    {
        _thresholdSeconds = Math.Max(0, thresholdSeconds);
        Win32.GetCursorPos(out _lastCursorPos);
        _timer = new Timer(Poll, null, PollIntervalMs, PollIntervalMs);
    }

    /// <summary>
    /// Update the idle threshold at runtime (e.g. when the user changes settings).
    /// </summary>
    public void UpdateThreshold(int thresholdSeconds)
    {
        _thresholdSeconds = Math.Max(0, thresholdSeconds);

        if (_thresholdSeconds == 0 && _idleActive)
        {
            _idleActive = false;
            IdleEnded?.Invoke(this, EventArgs.Empty);
        }
    }

    private void Poll(object? state)
    {
        if (_disposed)
            return;

        if (_thresholdSeconds <= 0)
            return;

        if (!Win32.GetCursorPos(out Win32.POINT current))
            return;

        if (current.x != _lastCursorPos.x || current.y != _lastCursorPos.y)
        {
            _lastCursorPos = current;
            _lastMoveTime  = DateTime.UtcNow;

            if (_idleActive)
            {
                _idleActive = false;
                Log.Debug("IdleModeService: idle ended (mouse moved)");
                IdleEnded?.Invoke(this, EventArgs.Empty);
            }
            return;
        }

        if (!_idleActive && (DateTime.UtcNow - _lastMoveTime).TotalSeconds >= _thresholdSeconds)
        {
            _idleActive = true;
            Log.Debug("IdleModeService: idle started (no movement for {Sec}s)", _thresholdSeconds);
            IdleStarted?.Invoke(this, EventArgs.Empty);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _timer.Dispose();
    }
}
