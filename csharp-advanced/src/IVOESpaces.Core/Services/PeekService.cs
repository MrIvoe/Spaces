using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using IVOESpaces.Core.Models;
using IVOESpaces.Shell;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 26: Manages Peek™ feature (Win+Space brings all spaces above other windows).
/// Handles z-order manipulation, state persistence, and window activation.
/// 
/// Peek temporarily lifts all space windows above application windows,
/// giving users a quick look at their desktop without minimizing everything.
/// </summary>
public sealed class PeekService
{
    private static PeekService? _instance;
    private static readonly object _lock = new();

    private readonly Dictionary<IntPtr, uint> _savedZOrder = new(); // hwnd -> original z-order info
    private bool _isPeekActive;
    private int _peekTimeoutMs = 3000; // Auto-close peek after 3 seconds

    public event EventHandler<PeekStateChangedEventArgs>? PeekStateChanged;

    private const int WM_ACTIVATEAPP = 0x001C;
    private const int WM_KEYUP = 0x0101;

    public static PeekService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    _instance ??= new PeekService();
                }
            }
            return _instance;
        }
    }

    public PeekService()
    {
        _isPeekActive = false;
    }

    /// <summary>
    /// Activates Peek mode: brings all spaces above active window(s).
    /// </summary>
    public void ActivatePeek()
    {
        if (_isPeekActive)
            return;

        _isPeekActive = true;
        _savedZOrder.Clear();

        try
        {
            // Find all space windows and bring to front
            var all = SpaceStateService.Instance.Spaces;
            var desktopHwnd = GetDesktopHwnd();

            if (desktopHwnd == IntPtr.Zero)
                throw new InvalidOperationException("Failed to locate desktop");

            // Save current z-order state before changing
            foreach (var space in all)
            {
                var spaceHwnd = FindSpaceWindow(space.Id);
                if (spaceHwnd != IntPtr.Zero)
                {
                    _savedZOrder[spaceHwnd] = GetZOrder(spaceHwnd);
                    // Bring space to top (HWND_TOP = 0)
                    NativeMethods.SetWindowPos(
                        spaceHwnd,
                        IntPtr.Zero, // HWND_TOP
                        0, 0, 0, 0,
                        0x0003 // SWP_NOMOVE | SWP_NOSIZE
                    );
                }
            }

            PeekStateChanged?.Invoke(this, new PeekStateChangedEventArgs
            {
                IsActive = true,
                SpaceCount = _savedZOrder.Count,
                TimeoutMs = _peekTimeoutMs
            });

            Serilog.Log.Information("Peek activated: {Count} spaces brought to front", _savedZOrder.Count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to activate Peek");
            DeactivatePeek();
        }
    }

    /// <summary>
    /// Deactivates Peek mode: restores windows to original z-order.
    /// </summary>
    public void DeactivatePeek()
    {
        if (!_isPeekActive || _savedZOrder.Count == 0)
            return;

        _isPeekActive = false;

        try
        {
            // Restore original z-order
            // Note: Simple approach - restore to HWND_BOTTOM to avoid conflicts
            // A more sophisticated approach would track insertion order
            foreach (var hwnd in _savedZOrder.Keys)
            {
                NativeMethods.SetWindowPos(
                    hwnd,
                    new IntPtr(-2), // HWND_BOTTOM
                    0, 0, 0, 0,
                    0x0003 // SWP_NOMOVE | SWP_NOSIZE
                );
            }

            _savedZOrder.Clear();
            _isPeekActive = false;

            PeekStateChanged?.Invoke(this, new PeekStateChangedEventArgs
            {
                IsActive = false,
                SpaceCount = 0,
                TimeoutMs = 0
            });

            Serilog.Log.Information("Peek deactivated: z-order restored");
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to deactivate Peek");
        }
    }

    /// <summary>
    /// Toggles Peek mode on/off.
    /// </summary>
    public void TogglePeek()
    {
        if (_isPeekActive)
            DeactivatePeek();
        else
            ActivatePeek();
    }

    /// <summary>
    /// Gets whether Peek is currently active.
    /// </summary>
    public bool IsPeekActive => _isPeekActive;

    /// <summary>
    /// Sets the timeout for auto-closing Peek mode (in milliseconds).
    /// </summary>
    public void SetPeekTimeout(int milliseconds)
    {
        _peekTimeoutMs = Math.Max(1000, milliseconds); // Minimum 1 second
        Serilog.Log.Debug("Peek timeout set to {Ms}ms", _peekTimeoutMs);
    }

    private IntPtr GetDesktopHwnd()
    {
        return NativeMethods.FindWindow("Progman", null);
    }

    private IntPtr FindSpaceWindow(Guid spaceId)
    {
        // TODO: Implement window search by space ID
        // For now, return zero (will be updated when SpaceWindow integration is added)
        return IntPtr.Zero;
    }

    private uint GetZOrder(IntPtr hwnd)
    {
        // Simplified z-order tracking
        // Returns the current position in z-order (lower = closer to user)
        var z = NativeMethods.GetWindow(hwnd, 3); // GW_HWNDPREV
        return (uint)z.ToInt64();
    }
}

/// <summary>
/// Event args for Peek state changes.
/// </summary>
public sealed class PeekStateChangedEventArgs : EventArgs
{
    public bool IsActive { get; set; }
    public int SpaceCount { get; set; }
    public int TimeoutMs { get; set; }
}
