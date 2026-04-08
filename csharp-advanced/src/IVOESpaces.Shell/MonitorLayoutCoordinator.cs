using IVOESpaces.Shell.Spaces;
using IVOESpaces.Shell.Native;
using IVOESpaces.Core.Services;
using IVOESpaces.Core.Models;
using Serilog;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace IVOESpaces.Shell;

/// <summary>
/// Handles monitor topology detection, display change events,
/// and space layout save/restore across monitor configurations.
/// Extracted from ShellHost to isolate monitor-related concerns.
/// </summary>
internal sealed class MonitorLayoutCoordinator
{
    internal delegate bool TryGetMonitorTopologyCallback(out List<string> monitors, out string configHash);

    private readonly Func<bool> _isReloadingFromState;
    private readonly Func<Task> _reloadFromStateAsync;
    private readonly Func<List<SpaceModel>> _captureCurrentSnapshot;
    private readonly Func<IEnumerable<SpaceModel>, Task> _replaceAllAsync;
    private readonly Func<string, List<SpaceModel>, Task> _saveLayoutSnapshotAsync;
    private readonly Func<string, Task<List<SpaceModel>>> _loadLayoutSnapshotAsync;
    private readonly Func<AppSettings> _getSettings;
    private readonly TryGetMonitorTopologyCallback _topologyProvider;
    private readonly SerialTaskQueue _displayChangeQueue = new();
    private string _lastMonitorConfigHash = string.Empty;

    public MonitorLayoutCoordinator(SpaceManager spaces)
        : this(
            () => spaces.IsReloadingFromState,
            spaces.ReloadFromStateAsync,
            () => SpaceStateService.Instance.Spaces.ToList(),
            spacesToReplace => SpaceStateService.Instance.ReplaceAllAsync(spacesToReplace),
            (configHash, spacesToSave) => SpaceRepository.Instance.SaveLayoutSnapshotAsync(configHash, spacesToSave),
            configHash => SpaceRepository.Instance.LoadLayoutSnapshotAsync(configHash),
            () => AppSettingsRepository.Instance.Current,
            TryGetMonitorTopology)
    {
    }

    internal MonitorLayoutCoordinator(
        Func<bool> isReloadingFromState,
        Func<Task> reloadFromStateAsync,
        Func<List<SpaceModel>> captureCurrentSnapshot,
        Func<IEnumerable<SpaceModel>, Task> replaceAllAsync,
        Func<string, List<SpaceModel>, Task> saveLayoutSnapshotAsync,
        Func<string, Task<List<SpaceModel>>> loadLayoutSnapshotAsync,
        Func<AppSettings> getSettings,
        TryGetMonitorTopologyCallback topologyProvider)
    {
        _isReloadingFromState = isReloadingFromState;
        _reloadFromStateAsync = reloadFromStateAsync;
        _captureCurrentSnapshot = captureCurrentSnapshot;
        _replaceAllAsync = replaceAllAsync;
        _saveLayoutSnapshotAsync = saveLayoutSnapshotAsync;
        _loadLayoutSnapshotAsync = loadLayoutSnapshotAsync;
        _getSettings = getSettings;
        _topologyProvider = topologyProvider;
    }

    public void InitializeHash()
    {
        if (_topologyProvider(out _, out string startupHash))
            _lastMonitorConfigHash = startupHash;
    }

    public void HandleDisplayChange()
    {
        _ = _displayChangeQueue.EnqueueSafe(
            HandleDisplayChangeAsync,
            ownershipName: "monitor display-change",
            ex => Log.Error(ex, "MonitorLayoutCoordinator: display-change queue operation failed"));
    }

    internal Task HandleDisplayChangeForTestingAsync() => _displayChangeQueue.EnqueueSafe(
        HandleDisplayChangeAsync,
        ownershipName: "monitor display-change test",
        ex => Log.Error(ex, "MonitorLayoutCoordinator: test display-change queue operation failed"));

    internal Task AwaitPendingDisplayChangeTaskAsync() => _displayChangeQueue.WhenIdleAsync();

    internal string LastMonitorConfigHashForTesting
    {
        get => _lastMonitorConfigHash;
        set => _lastMonitorConfigHash = value;
    }

    private async Task HandleDisplayChangeAsync()
    {
        try
        {
            if (_isReloadingFromState())
            {
                Log.Debug("MonitorLayoutCoordinator: display-change handling skipped while space reload is active");
                return;
            }

            AppSettings settings = _getSettings();
            if (!settings.DetectMonitorConfigurationChanges)
                return;

            if (!_topologyProvider(out List<string> monitors, out string newHash))
                return;

            if (string.IsNullOrEmpty(_lastMonitorConfigHash))
            {
                _lastMonitorConfigHash = newHash;
                return;
            }

            if (string.Equals(_lastMonitorConfigHash, newHash, StringComparison.Ordinal))
                return;

            string oldHash = _lastMonitorConfigHash;
            _lastMonitorConfigHash = newHash;

            List<SpaceModel> currentSnapshot = _captureCurrentSnapshot();
            await _saveLayoutSnapshotAsync(oldHash, currentSnapshot).ConfigureAwait(false);

            Log.Information(
                "MonitorLayoutCoordinator: monitor config changed old={OldHash} new={NewHash}; saved snapshot with {SpaceCount} spaces; monitors={Monitors}",
                oldHash, newHash, currentSnapshot.Count, string.Join(" | ", monitors));

            if (!settings.AutoSwapMisplacedSpaceContents)
                return;

            List<SpaceModel> restored = await _loadLayoutSnapshotAsync(newHash).ConfigureAwait(false);
            if (restored.Count == 0)
            {
                Log.Information("MonitorLayoutCoordinator: no saved snapshot found for monitor config {Hash}", newHash);
                return;
            }

            await _replaceAllAsync(restored).ConfigureAwait(false);
            await _reloadFromStateAsync().ConfigureAwait(false);
            Log.Information("MonitorLayoutCoordinator: restored {SpaceCount} spaces for monitor config {Hash}", restored.Count, newHash);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "MonitorLayoutCoordinator: display change handling failed");
        }
    }

    public static void LogStartupDiagnostics()
    {
        try
        {
            var settings = AppSettingsRepository.Instance.Current;
            Version? version = typeof(ShellHost).Assembly.GetName().Version;

            Log.Information(
                "Diagnostics: version={Version}, runtime={Runtime}, os={OS}, os64={Is64BitOS}, processArch={ProcessArch}",
                version?.ToString() ?? "unknown",
                RuntimeInformation.FrameworkDescription,
                RuntimeInformation.OSDescription,
                Environment.Is64BitOperatingSystem,
                RuntimeInformation.ProcessArchitecture);

            Log.Information(
                "Diagnostics: settings highDpiPerMonitor={HighDpi}, detectMonitorChanges={Detect}, autoSwapMisplaced={AutoSwap}, snapThreshold={SnapThreshold}",
                settings.HighDpiPerMonitorScaling,
                settings.DetectMonitorConfigurationChanges,
                settings.AutoSwapMisplacedSpaceContents,
                settings.SnapThreshold);

            if (!TryGetMonitorTopology(out List<string> monitors, out string monitorHash))
                return;

            Log.Information(
                "Diagnostics: monitorConfigHash={Hash}; monitors count={Count}; layout={Layout}",
                monitorHash, monitors.Count, string.Join(" | ", monitors));
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Diagnostics: failed to gather startup diagnostics");
        }
    }

    public static bool TryGetMonitorTopology(out List<string> monitors, out string configHash)
    {
        var discoveredMonitors = new List<string>();
        monitors = discoveredMonitors;
        configHash = string.Empty;

        try
        {
            bool Callback(IntPtr hMonitor, IntPtr _hdc, ref Win32.RECT _rect, IntPtr _data)
            {
                var mi = new Win32.MONITORINFOEX { cbSize = (uint)Marshal.SizeOf<Win32.MONITORINFOEX>() };
                if (!Win32.GetMonitorInfo(hMonitor, ref mi))
                    return true;

                bool primary = (mi.dwFlags & Win32.MONITORINFOF_PRIMARY) != 0;
                discoveredMonitors.Add(
                    $"{mi.szDevice}: monitor={mi.rcMonitor.Width}x{mi.rcMonitor.Height}@{mi.rcMonitor.left},{mi.rcMonitor.top}; work={mi.rcWork.Width}x{mi.rcWork.Height}@{mi.rcWork.left},{mi.rcWork.top}; primary={primary}");
                return true;
            }

            bool enumOk = Win32.EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, Callback, IntPtr.Zero);
            if (!enumOk)
            {
                Log.Warning("Diagnostics: EnumDisplayMonitors failed (err={Err})", Marshal.GetLastWin32Error());
                return false;
            }

            if (discoveredMonitors.Count == 0)
            {
                Log.Warning("Diagnostics: no monitors reported by EnumDisplayMonitors");
                return false;
            }

            discoveredMonitors.Sort(StringComparer.Ordinal);
            string canonical = string.Join("|", discoveredMonitors);
            byte[] hashBytes = SHA256.HashData(Encoding.UTF8.GetBytes(canonical));
            configHash = Convert.ToHexString(hashBytes)[..16];
            return true;
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Diagnostics: failed while computing monitor topology");
            return false;
        }
    }
}
