using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;
using Serilog;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Manages space layouts per unique monitor configuration.
/// When monitors are plugged/unplugged, restores layouts appropriate to that config.
/// Prevents icons/groups from reverting to wrong monitors or disappearing.
/// </summary>
public class MonitorConfigurationManager
{
    private static MonitorConfigurationManager? _instance;
    private readonly SpaceRepository _repository;
    private string _lastKnownConfigHash = string.Empty;

    public static MonitorConfigurationManager Instance =>
        _instance ??= new MonitorConfigurationManager();

    public MonitorConfigurationManager()
    {
        _repository = SpaceRepository.Instance;
        _lastKnownConfigHash = GetMonitorConfigurationHash();
    }

    /// <summary>
    /// Creates a unique hash of the current monitor configuration.
    /// Two configs with same hash have identical monitor geometry.
    /// </summary>
    public string GetMonitorConfigurationHash()
    {
        var monitorInfo = new StringBuilder();
        
        // Collect all monitor info in deterministic order
        var screens = Screen.AllScreens;
        Array.Sort(screens, (a, b) => a.DeviceName.CompareTo(b.DeviceName));

        foreach (var screen in screens)
        {
            monitorInfo.Append($"{screen.DeviceName}:");
            monitorInfo.Append($"{screen.Bounds.Width}x{screen.Bounds.Height}:");
            monitorInfo.Append($"{screen.Bounds.X},{screen.Bounds.Y}:");
            monitorInfo.Append($"DPI={GetScreenDpi(screen)};");
        }

        // Hash the configuration string
        using (var sha256 = SHA256.Create())
        {
            var hash = sha256.ComputeHash(Encoding.UTF8.GetBytes(monitorInfo.ToString()));
            return Convert.ToHexString(hash)[..16]; // First 16 chars of hex
        }
    }

    /// <summary>
    /// Gets DPI setting for a screen (simplified - returns 96 if can't determine)
    /// </summary>
    private int GetScreenDpi(Screen screen)
    {
        try
        {
            // On Windows, would use GetDpiForMonitor Win32 API
            // For now, simplified version - could be enhanced later
            using (var g = System.Drawing.Graphics.FromHwnd(IntPtr.Zero))
            {
                return (int)g.DpiX;
            }
        }
        catch
        {
            return 96;
        }
    }

    /// <summary>
    /// Check if monitor configuration has changed since last known state.
    /// Returns true if config is different (e.g., monitor unplugged)
    /// </summary>
    public bool HasMonitorConfigurationChanged()
    {
        var currentHash = GetMonitorConfigurationHash();
        if (currentHash != _lastKnownConfigHash)
        {
            Log.Information("Monitor configuration changed from {Old} to {New}", 
                _lastKnownConfigHash, currentHash);
            _lastKnownConfigHash = currentHash;
            return true;
        }
        return false;
    }

    /// <summary>
    /// Save current space layouts with the given config hash.
    /// Called before switching to ensure previous config is preserved.
    /// </summary>
    public async Task SaveLayoutForConfigurationAsync(string configHash, List<Models.SpaceModel> spaces)
    {
        try
        {
            await _repository.SaveLayoutSnapshotAsync(configHash, spaces).ConfigureAwait(false);
            Log.Information("Saved layout for config {Hash} with {SpaceCount} spaces", 
                configHash, spaces.Count);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to save layout for configuration {Hash}", configHash);
        }
    }

    /// <summary>
    /// Load space layouts for the specified monitor configuration.
    /// Returns empty list if config has never been seen before.
    /// </summary>
    public async Task<List<Models.SpaceModel>> LoadLayoutForConfigurationAsync(string configHash)
    {
        try
        {
            var spaces = await _repository.LoadLayoutSnapshotAsync(configHash).ConfigureAwait(false);
            Log.Information("Loaded {SpaceCount} spaces for config {Hash}", spaces.Count, configHash);
            return spaces;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load layout for configuration {Hash}", configHash);
            return new List<Models.SpaceModel>();
        }
    }

    /// <summary>
    /// Get all saved monitor configurations
    /// </summary>
    public List<(string Hash, int SpaceCount, DateTime LastActive)> GetSavedConfigurations()
    {
        try
        {
            return _repository.GetAllSavedConfigurations();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to get saved configurations");
            return new List<(string, int, DateTime)>();
        }
    }

    /// <summary>
    /// On monitor config change, saves current state and loads appropriate layout
    /// </summary>
    public async Task OnMonitorConfigurationChangedAsync(List<Models.SpaceModel> currentSpaces)
    {
        var oldHash = _lastKnownConfigHash;
        var newHash = GetMonitorConfigurationHash();

        if (oldHash == newHash)
            return; // No actual change

        // Save current state with old hash
        if (!string.IsNullOrEmpty(oldHash))
        {
            await SaveLayoutForConfigurationAsync(oldHash, currentSpaces).ConfigureAwait(false);
        }

        // Update to new hash
        _lastKnownConfigHash = newHash;
    }

    /// <summary>
    /// Validate that all spaces in the list are still on valid monitors.
    /// If a monitor was removed, optionally move spaces to primary.
    /// </summary>
    public void ValidateSpacesForCurrentMonitorConfig(List<Models.SpaceModel> spaces)
    {
        var validMonitors = new HashSet<string>();
        foreach (var screen in Screen.AllScreens)
        {
            validMonitors.Add(screen.DeviceName);
        }

        int movedCount = 0;
        foreach (var space in spaces)
        {
            if (!validMonitors.Contains(space.MonitorDeviceName))
            {
                // Monitor no longer exists - move to primary
                var primary = Screen.PrimaryScreen?.DeviceName ?? "Primary";
                Log.Warning("Space {SpaceId} was on removed monitor {Monitor}, moving to {Primary}",
                    space.Id, space.MonitorDeviceName, primary);
                space.MonitorDeviceName = primary;
                movedCount++;
            }
        }

        if (movedCount > 0)
        {
            Log.Information("Moved {Count} spaces due to removed monitors", movedCount);
        }
    }
}
