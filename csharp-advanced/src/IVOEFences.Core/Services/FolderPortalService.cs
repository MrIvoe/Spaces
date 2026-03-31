using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 31: Manages live folder portal functionality.
/// Fences can display real-time file system contents instead of static items.
/// Supports navigation (double-click to enter subfolder) and file system watching.
/// </summary>
public class FolderPortalService
{
    private const int LocalPortalTimeoutMs = 1000;
    private const int NetworkPortalTimeoutMs = 5000;
    private static readonly TimeSpan WatcherSignalThrottle = TimeSpan.FromMilliseconds(120);

    private readonly Dictionary<Guid, FileSystemWatcher> _watchers = new();
    private readonly Dictionary<Guid, string> _currentPaths = new(); // Track current nav path per fence
    private readonly Dictionary<Guid, PortalCacheEntry> _cachedItems = new();
    private readonly HashSet<Guid> _networkLoadsInFlight = new();
    private readonly Dictionary<Guid, int> _watcherGenerations = new();
    private readonly Dictionary<Guid, DateTime> _lastWatcherSignalUtc = new();
    private readonly object _sync = new();

    public event EventHandler<PortalItemsChangedEventArgs>? PortalItemsChanged;

    public sealed class PortalItemsChangedEventArgs : EventArgs
    {
        public Guid FenceId { get; init; }
        public string Path { get; init; } = string.Empty;
    }

    private sealed class PortalCacheEntry
    {
        public required string Path { get; init; }
        public required List<FenceItemModel> Items { get; init; }
    }

    public FolderPortalService() { }

    /// <summary>
    /// Step 31: Attach file system watcher to detect changes in portal folder.
    /// </summary>
    public void AttachWatcher(FenceModel fence)
    {
        if (fence.Type != FenceType.Portal || string.IsNullOrEmpty(fence.PortalFolderPath))
            return;

        try
        {
            ReplaceWatcher(fence.Id, fence.PortalFolderPath);

            Serilog.Log.Debug("Portal watcher attached for fence {FenceId} at {Path}",
                fence.Id, fence.PortalFolderPath);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Failed to attach portal watcher for fence {FenceId}", fence.Id);
        }
    }

    /// <summary>
    /// Step 31: Remove file system watcher when portal is closed.
    /// </summary>
    public void DetachWatcher(Guid fenceId)
    {
        FileSystemWatcher? watcher;
        lock (_sync)
        {
            _cachedItems.Remove(fenceId);
            _networkLoadsInFlight.Remove(fenceId);
            _watchers.TryGetValue(fenceId, out watcher);
            _watchers.Remove(fenceId);
            _currentPaths.Remove(fenceId);
            _watcherGenerations.Remove(fenceId);
            _lastWatcherSignalUtc.Remove(fenceId);
        }
        watcher?.Dispose(); // Dispose outside the lock to avoid holding lock during I/O
    }

    /// <summary>
    /// Step 31: Navigate into a subfolder (for double-click action).
    /// Returns true if navigation successful.
    /// </summary>
    public bool NavigateIntoFolder(Guid fenceId, string folderPath)
    {
        try
        {
            if (!Directory.Exists(folderPath))
                return false;

            ReplaceWatcher(fenceId, folderPath);

            Serilog.Log.Debug("Portal navigation: fence {FenceId} → {Path}", fenceId, folderPath);
            OnFileChanged(fenceId);
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Portal navigation failed for fence {FenceId}", fenceId);
            return false;
        }
    }

    /// <summary>
    /// Step 31: Navigate to parent folder (for back button).
    /// Returns true if navigation successful; false if already at root.
    /// </summary>
    public bool NavigateToParent(Guid fenceId, FenceModel fence)
    {
        string? currentPath;
        lock (_sync)
        {
            _currentPaths.TryGetValue(fenceId, out currentPath);
        }
        if (currentPath == null)
            return false;

        try
        {
            var parent = Directory.GetParent(currentPath);
            if (parent == null)
                return false;

            // Prevent going above the original portal root
            var portalRoot = new DirectoryInfo(fence.PortalFolderPath ?? string.Empty);
            if (portalRoot.FullName.Length > parent.FullName.Length)
                return false;

            return NavigateIntoFolder(fenceId, parent.FullName);
        }
        catch
        {
            return false;
        }
    }

    private void OnFileChanged(Guid fenceId)
    {
        // Step 31: Notify listeners that portal contents changed
        // UI should re-enumerate items
        PortalItemsChanged?.Invoke(this, new PortalItemsChangedEventArgs { FenceId = fenceId });
    }

    private void OnWatcherFileChanged(Guid fenceId, FileSystemWatcher sourceWatcher)
    {
        lock (_sync)
        {
            if (!_watchers.TryGetValue(fenceId, out FileSystemWatcher? currentWatcher) || !ReferenceEquals(currentWatcher, sourceWatcher))
                return;

            if (_lastWatcherSignalUtc.TryGetValue(fenceId, out DateTime last)
                && DateTime.UtcNow - last < WatcherSignalThrottle)
            {
                return;
            }

            _lastWatcherSignalUtc[fenceId] = DateTime.UtcNow;
        }

        OnFileChanged(fenceId);
    }

    private void OnWatcherError(Guid fenceId, FileSystemWatcher sourceWatcher, int generation, Exception? ex)
    {
        string? currentPath;
        lock (_sync)
        {
            if (!_watchers.TryGetValue(fenceId, out FileSystemWatcher? currentWatcher) || !ReferenceEquals(currentWatcher, sourceWatcher))
                return;

            if (!_watcherGenerations.TryGetValue(fenceId, out int currentGeneration) || currentGeneration != generation)
                return;

            if (!_currentPaths.TryGetValue(fenceId, out currentPath) || string.IsNullOrWhiteSpace(currentPath))
                return;
        }

        Serilog.Log.Warning(ex, "Portal watcher error for fence {FenceId} at {Path}; restarting watcher", fenceId, currentPath);

        try
        {
            ReplaceWatcher(fenceId, currentPath!);
            OnFileChanged(fenceId);
        }
        catch (Exception restartEx)
        {
            Serilog.Log.Warning(restartEx, "Portal watcher restart failed for fence {FenceId}", fenceId);
        }
    }

    private void ReplaceWatcher(Guid fenceId, string path)
    {
        int generation;
        FileSystemWatcher watcher = new(path)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite,
            EnableRaisingEvents = false,
        };

        lock (_sync)
        {
            generation = _watcherGenerations.TryGetValue(fenceId, out int existing) ? existing + 1 : 1;
            _watcherGenerations[fenceId] = generation;
        }

        watcher.Created += (_, _) => OnWatcherFileChanged(fenceId, watcher);
        watcher.Deleted += (_, _) => OnWatcherFileChanged(fenceId, watcher);
        watcher.Renamed += (_, _) => OnWatcherFileChanged(fenceId, watcher);
        watcher.Error += (_, e) => OnWatcherError(fenceId, watcher, generation, e.GetException());
        watcher.EnableRaisingEvents = true;

        FileSystemWatcher? oldWatcher;
        lock (_sync)
        {
            _watchers.TryGetValue(fenceId, out oldWatcher);
            _watchers[fenceId] = watcher;
            _currentPaths[fenceId] = path;
            _cachedItems.Remove(fenceId);
            _networkLoadsInFlight.Remove(fenceId);
            _lastWatcherSignalUtc.Remove(fenceId);
        }

        oldWatcher?.Dispose();
    }

    /// <summary>
    /// Step 31: Get current items visible in portal (files + folders from current navigation path).
    /// </summary>
    public IEnumerable<FenceItemModel> EnumeratePortalItems(FenceModel fence)
    {
        if (fence.Type != FenceType.Portal || string.IsNullOrEmpty(fence.PortalFolderPath))
            return Array.Empty<FenceItemModel>();

        string currentPath;
        lock (_sync)
        {
            currentPath = _currentPaths.TryGetValue(fence.Id, out var path)
                ? path
                : fence.PortalFolderPath;
        }

        int timeoutMs = IsNetworkPath(currentPath) ? NetworkPortalTimeoutMs : LocalPortalTimeoutMs;

        if (IsNetworkPath(currentPath))
            return EnumerateNetworkPortalItems(fence, currentPath, timeoutMs);

        bool exists = RunWithTimeout(
            () => Directory.Exists(currentPath),
            timeoutMs,
            fallbackValue: false,
            operationName: "Directory.Exists",
            fenceId: fence.Id,
            targetPath: currentPath);
        if (!exists)
            return CreateUnavailablePlaceholder(currentPath, "Folder unavailable");

        List<FenceItemModel> result = RunWithTimeout(
            () => EnumeratePortalItemsCore(fence, currentPath),
            timeoutMs,
            fallbackValue: CreateUnavailablePlaceholder(currentPath, "Portal load timed out"),
            operationName: "EnumeratePortalItems",
            fenceId: fence.Id,
            targetPath: currentPath);

        lock (_sync)
        {
            _cachedItems[fence.Id] = new PortalCacheEntry
            {
                Path = currentPath,
                Items = result,
            };
        }

        return result;
    }

    private IEnumerable<FenceItemModel> EnumerateNetworkPortalItems(FenceModel fence, string currentPath, int timeoutMs)
    {
        lock (_sync)
        {
            if (_cachedItems.TryGetValue(fence.Id, out PortalCacheEntry? cache)
                && string.Equals(cache.Path, currentPath, StringComparison.OrdinalIgnoreCase)
                && cache.Items.Count > 0)
            {
                return new List<FenceItemModel>(cache.Items);
            }

            if (!_networkLoadsInFlight.Contains(fence.Id))
            {
                _networkLoadsInFlight.Add(fence.Id);
                _ = RefreshNetworkPortalItemsAsync(fence, currentPath, timeoutMs);
            }
        }

        return CreateUnavailablePlaceholder(currentPath, "Loading folder...");
    }

    private async Task RefreshNetworkPortalItemsAsync(FenceModel fence, string currentPath, int timeoutMs)
    {
        List<FenceItemModel> items;

        try
        {
            var enumerateTask = Task.Run(() => EnumeratePortalItemsCore(fence, currentPath));
            Task completed = await Task.WhenAny(enumerateTask, Task.Delay(timeoutMs)).ConfigureAwait(false);

            if (completed == enumerateTask)
            {
                items = enumerateTask.Result;
            }
            else
            {
                Serilog.Log.Warning(
                    "Portal operation timed out: {Operation} fence={FenceId} timeoutMs={Timeout} path={Path}",
                    "EnumeratePortalItemsAsync",
                    fence.Id,
                    timeoutMs,
                    currentPath);
                items = CreateUnavailablePlaceholder(currentPath, "Portal load timed out");
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(
                ex,
                "Portal operation failed: {Operation} fence={FenceId} path={Path}",
                "EnumeratePortalItemsAsync",
                fence.Id,
                currentPath);
            items = CreateUnavailablePlaceholder(currentPath, "Folder unavailable");
        }

        lock (_sync)
        {
            _cachedItems[fence.Id] = new PortalCacheEntry
            {
                Path = currentPath,
                Items = items,
            };
            _networkLoadsInFlight.Remove(fence.Id);
        }

        OnFileChanged(fence.Id);
    }

    private static List<FenceItemModel> EnumeratePortalItemsCore(FenceModel fence, string currentPath)
    {
        var result = new List<FenceItemModel>();

        // Step 31: Add parent folder marker if not at root
        var portalRoot = new DirectoryInfo(fence.PortalFolderPath!);
        var currentDir = new DirectoryInfo(currentPath);
        if (currentDir.FullName != portalRoot.FullName)
        {
            result.Add(new FenceItemModel
            {
                Id = Guid.Empty, // Special marker for back button
                DisplayName = "...",
                TargetPath = currentDir.Parent?.FullName ?? currentPath,
                IsDirectory = true
            });
        }

        // Step 31: Add subdirectories
        foreach (var dir in Directory.EnumerateDirectories(currentPath))
        {
            var dirInfo = new DirectoryInfo(dir);
            result.Add(new FenceItemModel
            {
                DisplayName = dirInfo.Name,
                TargetPath = dir,
                IsDirectory = true
            });
        }

        // Step 31: Add files
        foreach (var file in Directory.EnumerateFiles(currentPath))
        {
            result.Add(new FenceItemModel
            {
                DisplayName = Path.GetFileNameWithoutExtension(file),
                TargetPath = file,
                IsDirectory = false
            });
        }

        return result;
    }

    private static bool IsNetworkPath(string path)
    {
        if (path.StartsWith("\\\\", StringComparison.Ordinal))
            return true;

        try
        {
            string root = Path.GetPathRoot(path) ?? string.Empty;
            if (string.IsNullOrWhiteSpace(root))
                return false;

            var drive = new DriveInfo(root);
            return drive.DriveType == DriveType.Network;
        }
        catch
        {
            return false;
        }
    }

    private static T RunWithTimeout<T>(
        Func<T> operation,
        int timeoutMs,
        T fallbackValue,
        string operationName,
        Guid fenceId,
        string targetPath)
    {
        try
        {
            var task = Task.Run(operation);
            if (task.Wait(timeoutMs))
                return task.Result;

            Serilog.Log.Warning(
                "Portal operation timed out: {Operation} fence={FenceId} timeoutMs={Timeout} path={Path}",
                operationName,
                fenceId,
                timeoutMs,
                targetPath);
            return fallbackValue;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(
                ex,
                "Portal operation failed: {Operation} fence={FenceId} path={Path}",
                operationName,
                fenceId,
                targetPath);
            return fallbackValue;
        }
    }

    private static List<FenceItemModel> CreateUnavailablePlaceholder(string path, string reason)
    {
        return new List<FenceItemModel>
        {
            new()
            {
                DisplayName = reason,
                TargetPath = path,
                IsDirectory = true,
                IsUnresolved = true,
            }
        };
    }
}
