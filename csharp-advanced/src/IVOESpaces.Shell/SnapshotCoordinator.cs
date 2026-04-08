using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Debounces automatic snapshot creation and owns the shell timer contract.
/// </summary>
internal sealed class SnapshotCoordinator : IDisposable
{
    private readonly TimeSpan _debounce;
    private readonly Func<IntPtr> _timerOwnerProvider;
    private readonly IntPtr _timerId;
    private bool _pending;

    public SnapshotCoordinator(TimeSpan debounce, Func<IntPtr> timerOwnerProvider, int timerId = 9002)
    {
        _debounce = debounce;
        _timerOwnerProvider = timerOwnerProvider;
        _timerId = new IntPtr(timerId);
    }

    public void OnStateChanged()
    {
        IntPtr owner = _timerOwnerProvider();
        if (owner == IntPtr.Zero)
            return;

        _pending = true;
        Win32.SetTimer(owner, _timerId, (uint)_debounce.TotalMilliseconds, IntPtr.Zero);
    }

    public void OnStatePossiblyChanged()
    {
        OnStateChanged();
    }

    public bool HandleTimer(IntPtr hwnd, IntPtr wParam)
    {
        if (wParam != _timerId)
            return false;

        IntPtr owner = _timerOwnerProvider();
        if (owner == IntPtr.Zero || hwnd != owner)
            return false;

        Win32.KillTimer(owner, _timerId);

        if (!_pending)
            return true;

        _pending = false;

        try
        {
            SnapshotRepository.Instance.CreateAutoBackup();
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "SnapshotCoordinator: auto snapshot failed");
        }

        return true;
    }

    public void Dispose()
    {
        IntPtr owner = _timerOwnerProvider();
        if (owner != IntPtr.Zero)
            Win32.KillTimer(owner, _timerId);
    }
}