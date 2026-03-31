using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Watches the user's desktop folder for file-system changes (new files, renames, deletes).
/// Raises events so the UI layer can auto-capture new items into fences.
/// </summary>
public sealed class DesktopWatcherService : IDisposable
{
    private static DesktopWatcherService? _instance;
    private static readonly object _lock = new();

    private FileSystemWatcher? _userWatcher;
    private FileSystemWatcher? _commonWatcher;
    private readonly string _userDesktopPath;
    private readonly string _commonDesktopPath;
    private readonly Func<TimeSpan, Task> _delayAsync;
    private readonly object _lifecycleLock = new();
    private readonly object _restartTaskGate = new();
    private bool _disposed;
    private int _pauseCounter;
    private int _restartAttempts;
    private int _watcherGeneration;
    private int _restartScheduled;
    private Task _pendingRestartTask = Task.CompletedTask;

    /// <summary>Fires when a new file or shortcut appears on the desktop.</summary>
    public event EventHandler<DesktopItemEventArgs>? ItemCreated;

    /// <summary>Fires when a desktop file is deleted.</summary>
    public event EventHandler<DesktopItemEventArgs>? ItemDeleted;

    /// <summary>Fires when a desktop file is renamed.</summary>
    public event EventHandler<DesktopItemRenamedEventArgs>? ItemRenamed;

    public static DesktopWatcherService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    _instance ??= new DesktopWatcherService();
                }
            }
            return _instance;
        }
    }

    private DesktopWatcherService()
        : this(
            Environment.GetFolderPath(Environment.SpecialFolder.Desktop),
            Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory),
            delayAsync: null)
    {
    }

    internal DesktopWatcherService(string userDesktopPath, string commonDesktopPath, Func<TimeSpan, Task>? delayAsync = null)
    {
        _userDesktopPath = userDesktopPath;
        _commonDesktopPath = commonDesktopPath;
        _delayAsync = delayAsync ?? (delay => Task.Delay(delay));
    }

    public static DesktopWatcherService CreateForTesting(
        string userDesktopPath,
        string commonDesktopPath,
        Func<TimeSpan, Task>? delayAsync = null)
    {
        return new DesktopWatcherService(userDesktopPath, commonDesktopPath, delayAsync);
    }

    internal FileSystemWatcher? UserWatcherForTesting => _userWatcher;

    internal FileSystemWatcher? CommonWatcherForTesting => _commonWatcher;

    internal int WatcherGenerationForTesting => Volatile.Read(ref _watcherGeneration);

    internal int RestartAttemptsForTesting => Volatile.Read(ref _restartAttempts);

    internal int RestartScheduledForTesting => Volatile.Read(ref _restartScheduled);

    internal Task RestartAfterErrorForTestingAsync(FileSystemWatcher watcher, int generation, Exception? error)
    {
        return RestartAfterErrorAsync(watcher, generation, error);
    }

    internal Task AwaitPendingRestartTaskAsync()
    {
        lock (_restartTaskGate)
            return _pendingRestartTask;
    }

    /// <summary>
    /// Start monitoring the desktop folder. Idempotent.
    /// </summary>
    public void Start()
    {
        lock (_lifecycleLock)
        {
            if (_disposed)
                return;

            if (_userWatcher != null || _commonWatcher != null)
                return;

            int generation = Interlocked.Increment(ref _watcherGeneration);
            _restartScheduled = 0;

            _userWatcher = TryCreateWatcher(_userDesktopPath, generation);

            if (!string.IsNullOrEmpty(_commonDesktopPath)
                && Directory.Exists(_commonDesktopPath)
                && !string.Equals(_userDesktopPath, _commonDesktopPath, StringComparison.OrdinalIgnoreCase))
            {
                _commonWatcher = TryCreateWatcher(_commonDesktopPath, generation);
            }

            _restartAttempts = 0;
        }

        Serilog.Log.Information("DesktopWatcherService started monitoring {UserPath} and {CommonPath}",
            _userDesktopPath, _commonDesktopPath);
    }

    private FileSystemWatcher? TryCreateWatcher(string path, int generation)
    {
        try
        {
            var watcher = new FileSystemWatcher(path)
            {
                NotifyFilter = NotifyFilters.FileName
                             | NotifyFilters.DirectoryName
                             | NotifyFilters.LastWrite,
                IncludeSubdirectories = false,
                EnableRaisingEvents = true
            };

            watcher.Created += (_, e) => OnCreated(watcher, e, generation);
            watcher.Deleted += (_, e) => OnDeleted(watcher, e, generation);
            watcher.Renamed += (_, e) => OnRenamed(watcher, e, generation);
            watcher.Error += (_, e) => OnError(watcher, e, generation);

            return watcher;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to create watcher for {Path}", path);
            return null;
        }
    }

    /// <summary>
    /// Stop monitoring.
    /// </summary>
    public void Stop()
    {
        StopCore(resetRestartScheduled: true);
        Serilog.Log.Information("DesktopWatcherService stopped");
    }

    private void StopCore(bool resetRestartScheduled)
    {
        lock (_lifecycleLock)
        {
            Interlocked.Increment(ref _watcherGeneration);
            if (resetRestartScheduled)
                _restartScheduled = 0;
            DisposeWatcher(ref _userWatcher);
            DisposeWatcher(ref _commonWatcher);
        }
    }

    private static void DisposeWatcher(ref FileSystemWatcher? watcher)
    {
        if (watcher == null) return;
        watcher.EnableRaisingEvents = false;
        watcher.Dispose();
        watcher = null;
    }

    public void Pause()
    {
        Interlocked.Increment(ref _pauseCounter);
        Serilog.Log.Debug("DesktopWatcherService paused (depth={Depth})", _pauseCounter);
    }

    public void Resume()
    {
        int depth = Interlocked.Decrement(ref _pauseCounter);
        if (depth < 0)
            Interlocked.Exchange(ref _pauseCounter, 0);
        Serilog.Log.Debug("DesktopWatcherService resumed (depth={Depth})", Math.Max(depth, 0));
    }

    private bool IsPaused => Volatile.Read(ref _pauseCounter) > 0;

    /// <summary>
    /// Get all files currently on the desktop (initial scan).
    /// </summary>
    public List<FenceItemModel> ScanDesktop()
    {
        var items = new List<FenceItemModel>();
        ScanPath(_userDesktopPath, items);
        if (!string.IsNullOrEmpty(_commonDesktopPath)
            && Directory.Exists(_commonDesktopPath)
            && !string.Equals(_userDesktopPath, _commonDesktopPath, StringComparison.OrdinalIgnoreCase))
        {
            ScanPath(_commonDesktopPath, items);
        }
        return items;
    }

    private static void ScanPath(string path, List<FenceItemModel> items)
    {
        try
        {
            foreach (var entry in Directory.EnumerateFileSystemEntries(path))
            {
                var item = CreateItemFromPath(entry);
                if (item != null)
                    items.Add(item);
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to scan desktop path {Path}", path);
        }
    }

    private bool IsCurrentWatcher(FileSystemWatcher watcher, int generation)
    {
        if (generation != Volatile.Read(ref _watcherGeneration))
            return false;

        lock (_lifecycleLock)
            return ReferenceEquals(_userWatcher, watcher) || ReferenceEquals(_commonWatcher, watcher);
    }

    private void OnCreated(FileSystemWatcher watcher, FileSystemEventArgs e, int generation)
    {
        if (!IsCurrentWatcher(watcher, generation)) return;
        if (IsPaused) return;
        // Skip hidden/system files and desktop.ini
        if (ShouldIgnore(e.FullPath)) return;

        var item = CreateItemFromPath(e.FullPath);
        if (item != null)
        {
            // Notify subscribers
            ItemCreated?.Invoke(this, new DesktopItemEventArgs { Item = item, FullPath = e.FullPath });
            
            // Update entity registry and reconcile
            FenceItemReconciliationService.Instance.OnDesktopItemCreated(e.FullPath);
            
            Serilog.Log.Debug("Desktop item created: {Path}", e.Name);
        }
    }

    private void OnDeleted(FileSystemWatcher watcher, FileSystemEventArgs e, int generation)
    {
        if (!IsCurrentWatcher(watcher, generation)) return;
        if (IsPaused) return;
        DesktopEntityModel? entity = DesktopEntityRegistryService.Instance.TryGetByPath(e.FullPath);
        ItemDeleted?.Invoke(this, new DesktopItemEventArgs
        {
            Item = new FenceItemModel
            {
                Id = Guid.NewGuid(),
                DesktopEntityId = entity?.Id ?? Guid.Empty,
                DisplayName = Path.GetFileNameWithoutExtension(e.Name ?? ""),
                TargetPath = e.FullPath
            },
            FullPath = e.FullPath
        });
        
        // Update entity registry and reconcile
        FenceItemReconciliationService.Instance.OnDesktopItemDeleted(e.FullPath);
        
        Serilog.Log.Debug("Desktop item deleted: {Path}", e.Name);
    }

    private void OnRenamed(FileSystemWatcher watcher, RenamedEventArgs e, int generation)
    {
        if (!IsCurrentWatcher(watcher, generation)) return;
        if (IsPaused) return;
        DesktopEntityRegistryService.Instance.HandleRename(e.OldFullPath, e.FullPath, Path.GetFileNameWithoutExtension(e.Name ?? ""));
        ItemRenamed?.Invoke(this, new DesktopItemRenamedEventArgs
        {
            OldPath = e.OldFullPath,
            NewPath = e.FullPath,
            OldName = Path.GetFileNameWithoutExtension(e.OldName ?? ""),
            NewName = Path.GetFileNameWithoutExtension(e.Name ?? "")
        });
        
        // Update entity registry and reconcile
        FenceItemReconciliationService.Instance.OnDesktopItemRenamed(e.OldFullPath, e.FullPath);
        
        Serilog.Log.Debug("Desktop item renamed: {Old} → {New}", e.OldName, e.Name);
    }

    private void OnError(FileSystemWatcher watcher, ErrorEventArgs e, int generation)
    {
        lock (_restartTaskGate)
        {
            _pendingRestartTask = RestartAfterErrorSafelyAsync(watcher, generation, e.GetException());
        }
    }

    private async Task RestartAfterErrorSafelyAsync(FileSystemWatcher watcher, int generation, Exception? error)
    {
        try
        {
            await RestartAfterErrorAsync(watcher, generation, error).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "DesktopWatcherService: unhandled restart failure for generation {Generation}", generation);
        }
    }

    private async Task RestartAfterErrorAsync(FileSystemWatcher watcher, int generation, Exception? error)
    {
        if (_disposed)
            return;

        if (!IsCurrentWatcher(watcher, generation))
            return;

        if (Interlocked.CompareExchange(ref _restartScheduled, 1, 0) != 0)
            return;

        int observedGeneration = Volatile.Read(ref _watcherGeneration);
        int attempts = Interlocked.Increment(ref _restartAttempts);
        int delayMs = Math.Min(30_000, 1_000 * attempts);
        Serilog.Log.Warning(
            error,
            "DesktopWatcherService: watcher error for generation {Generation} — restarting in {DelayMs}ms (attempt {Attempt})",
            generation,
            delayMs,
            attempts);

        try
        {
            StopCore(resetRestartScheduled: false);
            await _delayAsync(TimeSpan.FromMilliseconds(delayMs)).ConfigureAwait(false);

            if (_disposed)
                return;

            // Only restart if no newer lifecycle transition happened in the meantime.
            if (Volatile.Read(ref _watcherGeneration) != observedGeneration + 1)
            {
                Serilog.Log.Debug(
                    "DesktopWatcherService: skipped restart because generation advanced from {Observed} to {Current}",
                    observedGeneration + 1,
                    Volatile.Read(ref _watcherGeneration));
                return;
            }

            Start();
            Serilog.Log.Information(
                "DesktopWatcherService: watcher restart completed for generation {Generation}",
                Volatile.Read(ref _watcherGeneration));
        }
        finally
        {
            Interlocked.Exchange(ref _restartScheduled, 0);
        }
    }

    private static bool ShouldIgnore(string path)
    {
        var name = Path.GetFileName(path);
        if (string.Equals(name, "desktop.ini", StringComparison.OrdinalIgnoreCase))
            return true;

        try
        {
            var attr = File.GetAttributes(path);
            if (attr.HasFlag(FileAttributes.Hidden) || attr.HasFlag(FileAttributes.System))
                return true;
        }
        catch { }

        return false;
    }

    private static FenceItemModel? CreateItemFromPath(string path)
    {
        try
        {
            if (ShouldIgnore(path)) return null;

            bool isDirectory = Directory.Exists(path);
            DesktopEntityModel entity = DesktopEntityRegistryService.Instance.EnsureEntity(
                path,
                Path.GetFileNameWithoutExtension(path),
                isDirectory);

            return new FenceItemModel
            {
                Id = Guid.NewGuid(),
                DesktopEntityId = entity.Id,
                DisplayName = entity.DisplayName,
                TargetPath = path,
                IsDirectory = isDirectory
            };
        }
        catch
        {
            return null;
        }
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _disposed = true;
            Stop();
        }
    }

    /// <summary>
    /// Returns a disposable scope that pauses the watcher while alive.
    /// Usage: using (DesktopWatcherService.Instance.PauseScope()) { … }
    /// </summary>
    public DesktopWatcherPauseScope PauseScope() => new(this);
}

/// <summary>
/// RAII scope that pauses the <see cref="DesktopWatcherService"/> while alive.
/// Prevents spurious file-system events during ownership moves.
/// </summary>
public sealed class DesktopWatcherPauseScope : IDisposable
{
    private readonly DesktopWatcherService _service;
    private bool _disposed;

    internal DesktopWatcherPauseScope(DesktopWatcherService service)
    {
        _service = service;
        _service.Pause();
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _disposed = true;
            _service.Resume();
        }
    }
}

public sealed class DesktopItemEventArgs : EventArgs
{
    public FenceItemModel Item { get; init; } = null!;
    public string FullPath { get; init; } = string.Empty;
}

public sealed class DesktopItemRenamedEventArgs : EventArgs
{
    public string OldPath { get; init; } = string.Empty;
    public string NewPath { get; init; } = string.Empty;
    public string OldName { get; init; } = string.Empty;
    public string NewName { get; init; } = string.Empty;
}
