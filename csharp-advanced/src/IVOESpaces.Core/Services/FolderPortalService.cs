using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 31: Manages live folder portal functionality.
/// Spaces can display real-time file system contents instead of static items.
/// Supports navigation (double-click to enter subfolder) and file system watching.
/// </summary>
public class FolderPortalService
{
    private const int LocalPortalTimeoutMs = 1000;
    private const int NetworkPortalTimeoutMs = 5000;
    private static readonly TimeSpan WatcherSignalThrottle = TimeSpan.FromMilliseconds(120);

    private readonly Dictionary<Guid, FileSystemWatcher> _watchers = new();
    private readonly Dictionary<Guid, string> _currentPaths = new(); // Track current nav path per space
    private readonly Dictionary<Guid, PortalCacheEntry> _cachedItems = new();
    private readonly HashSet<Guid> _networkLoadsInFlight = new();
    private readonly Dictionary<Guid, int> _watcherGenerations = new();
    private readonly Dictionary<Guid, DateTime> _lastWatcherSignalUtc = new();
    private readonly object _sync = new();

    public event EventHandler<PortalItemsChangedEventArgs>? PortalItemsChanged;

    public sealed class PortalItemsChangedEventArgs : EventArgs
    {
        public Guid SpaceId { get; init; }
        public string Path { get; init; } = string.Empty;
    }

    private sealed class PortalCacheEntry
    {
        public required string Path { get; init; }
        public required List<SpaceItemModel> Items { get; init; }
    }

    public FolderPortalService() { }

    /// <summary>
    /// Step 31: Attach file system watcher to detect changes in portal folder.
    /// </summary>
    public void AttachWatcher(SpaceModel space)
    {
        if (space.Type != SpaceType.Portal || string.IsNullOrEmpty(space.PortalFolderPath))
            return;

        try
        {
            ReplaceWatcher(space.Id, space.PortalFolderPath);

            Serilog.Log.Debug("Portal watcher attached for space {SpaceId} at {Path}",
                space.Id, space.PortalFolderPath);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Failed to attach portal watcher for space {SpaceId}", space.Id);
        }
    }

    /// <summary>
    /// Step 31: Remove file system watcher when portal is closed.
    /// </summary>
    public void DetachWatcher(Guid spaceId)
    {
        FileSystemWatcher? watcher;
        lock (_sync)
        {
            _cachedItems.Remove(spaceId);
            _networkLoadsInFlight.Remove(spaceId);
            _watchers.TryGetValue(spaceId, out watcher);
            _watchers.Remove(spaceId);
            _currentPaths.Remove(spaceId);
            _watcherGenerations.Remove(spaceId);
            _lastWatcherSignalUtc.Remove(spaceId);
        }
        watcher?.Dispose(); // Dispose outside the lock to avoid holding lock during I/O
    }

    /// <summary>
    /// Step 31: Navigate into a subfolder (for double-click action).
    /// Returns true if navigation successful.
    /// </summary>
    public bool NavigateIntoFolder(Guid spaceId, string folderPath)
    {
        try
        {
            if (!Directory.Exists(folderPath))
                return false;

            ReplaceWatcher(spaceId, folderPath);

            Serilog.Log.Debug("Portal navigation: space {SpaceId} → {Path}", spaceId, folderPath);
            OnFileChanged(spaceId);
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Portal navigation failed for space {SpaceId}", spaceId);
            return false;
        }
    }

    /// <summary>
    /// Step 31: Navigate to parent folder (for back button).
    /// Returns true if navigation successful; false if already at root.
    /// </summary>
    public bool NavigateToParent(Guid spaceId, SpaceModel space)
    {
        string? currentPath;
        lock (_sync)
        {
            _currentPaths.TryGetValue(spaceId, out currentPath);
        }
        if (currentPath == null)
            return false;

        try
        {
            var parent = Directory.GetParent(currentPath);
            if (parent == null)
                return false;

            // Prevent going above the original portal root
            var portalRoot = new DirectoryInfo(space.PortalFolderPath ?? string.Empty);
            if (portalRoot.FullName.Length > parent.FullName.Length)
                return false;

            return NavigateIntoFolder(spaceId, parent.FullName);
        }
        catch
        {
            return false;
        }
    }

    private void OnFileChanged(Guid spaceId)
    {
        // Step 31: Notify listeners that portal contents changed
        // UI should re-enumerate items
        PortalItemsChanged?.Invoke(this, new PortalItemsChangedEventArgs { SpaceId = spaceId });
    }

    private void OnWatcherFileChanged(Guid spaceId, FileSystemWatcher sourceWatcher)
    {
        lock (_sync)
        {
            if (!_watchers.TryGetValue(spaceId, out FileSystemWatcher? currentWatcher) || !ReferenceEquals(currentWatcher, sourceWatcher))
                return;

            if (_lastWatcherSignalUtc.TryGetValue(spaceId, out DateTime last)
                && DateTime.UtcNow - last < WatcherSignalThrottle)
            {
                return;
            }

            _lastWatcherSignalUtc[spaceId] = DateTime.UtcNow;
        }

        OnFileChanged(spaceId);
    }

    private void OnWatcherError(Guid spaceId, FileSystemWatcher sourceWatcher, int generation, Exception? ex)
    {
        string? currentPath;
        lock (_sync)
        {
            if (!_watchers.TryGetValue(spaceId, out FileSystemWatcher? currentWatcher) || !ReferenceEquals(currentWatcher, sourceWatcher))
                return;

            if (!_watcherGenerations.TryGetValue(spaceId, out int currentGeneration) || currentGeneration != generation)
                return;

            if (!_currentPaths.TryGetValue(spaceId, out currentPath) || string.IsNullOrWhiteSpace(currentPath))
                return;
        }

        Serilog.Log.Warning(ex, "Portal watcher error for space {SpaceId} at {Path}; restarting watcher", spaceId, currentPath);

        try
        {
            ReplaceWatcher(spaceId, currentPath!);
            OnFileChanged(spaceId);
        }
        catch (Exception restartEx)
        {
            Serilog.Log.Warning(restartEx, "Portal watcher restart failed for space {SpaceId}", spaceId);
        }
    }

    private void ReplaceWatcher(Guid spaceId, string path)
    {
        int generation;
        FileSystemWatcher watcher = new(path)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite,
            EnableRaisingEvents = false,
        };

        lock (_sync)
        {
            generation = _watcherGenerations.TryGetValue(spaceId, out int existing) ? existing + 1 : 1;
            _watcherGenerations[spaceId] = generation;
        }

        watcher.Created += (_, _) => OnWatcherFileChanged(spaceId, watcher);
        watcher.Deleted += (_, _) => OnWatcherFileChanged(spaceId, watcher);
        watcher.Renamed += (_, _) => OnWatcherFileChanged(spaceId, watcher);
        watcher.Error += (_, e) => OnWatcherError(spaceId, watcher, generation, e.GetException());
        watcher.EnableRaisingEvents = true;

        FileSystemWatcher? oldWatcher;
        lock (_sync)
        {
            _watchers.TryGetValue(spaceId, out oldWatcher);
            _watchers[spaceId] = watcher;
            _currentPaths[spaceId] = path;
            _cachedItems.Remove(spaceId);
            _networkLoadsInFlight.Remove(spaceId);
            _lastWatcherSignalUtc.Remove(spaceId);
        }

        oldWatcher?.Dispose();
    }

    /// <summary>
    /// Step 31: Get current items visible in portal (files + folders from current navigation path).
    /// </summary>
    public IEnumerable<SpaceItemModel> EnumeratePortalItems(SpaceModel space)
    {
        if (space.Type != SpaceType.Portal || string.IsNullOrEmpty(space.PortalFolderPath))
            return Array.Empty<SpaceItemModel>();

        string currentPath;
        lock (_sync)
        {
            currentPath = _currentPaths.TryGetValue(space.Id, out var path)
                ? path
                : space.PortalFolderPath;
        }

        int timeoutMs = IsNetworkPath(currentPath) ? NetworkPortalTimeoutMs : LocalPortalTimeoutMs;

        if (IsNetworkPath(currentPath))
            return EnumerateNetworkPortalItems(space, currentPath, timeoutMs);

        bool exists = RunWithTimeout(
            () => Directory.Exists(currentPath),
            timeoutMs,
            fallbackValue: false,
            operationName: "Directory.Exists",
            spaceId: space.Id,
            targetPath: currentPath);
        if (!exists)
            return CreateUnavailablePlaceholder(currentPath, "Folder unavailable");

        List<SpaceItemModel> result = RunWithTimeout(
            () => EnumeratePortalItemsCore(space, currentPath),
            timeoutMs,
            fallbackValue: CreateUnavailablePlaceholder(currentPath, "Portal load timed out"),
            operationName: "EnumeratePortalItems",
            spaceId: space.Id,
            targetPath: currentPath);

        lock (_sync)
        {
            _cachedItems[space.Id] = new PortalCacheEntry
            {
                Path = currentPath,
                Items = result,
            };
        }

        return result;
    }

    private IEnumerable<SpaceItemModel> EnumerateNetworkPortalItems(SpaceModel space, string currentPath, int timeoutMs)
    {
        lock (_sync)
        {
            if (_cachedItems.TryGetValue(space.Id, out PortalCacheEntry? cache)
                && string.Equals(cache.Path, currentPath, StringComparison.OrdinalIgnoreCase)
                && cache.Items.Count > 0)
            {
                return new List<SpaceItemModel>(cache.Items);
            }

            if (!_networkLoadsInFlight.Contains(space.Id))
            {
                _networkLoadsInFlight.Add(space.Id);
                _ = RefreshNetworkPortalItemsAsync(space, currentPath, timeoutMs);
            }
        }

        return CreateUnavailablePlaceholder(currentPath, "Loading folder...");
    }

    private async Task RefreshNetworkPortalItemsAsync(SpaceModel space, string currentPath, int timeoutMs)
    {
        List<SpaceItemModel> items;

        try
        {
            var enumerateTask = Task.Run(() => EnumeratePortalItemsCore(space, currentPath));
            Task completed = await Task.WhenAny(enumerateTask, Task.Delay(timeoutMs)).ConfigureAwait(false);

            if (completed == enumerateTask)
            {
                items = enumerateTask.Result;
            }
            else
            {
                Serilog.Log.Warning(
                    "Portal operation timed out: {Operation} space={SpaceId} timeoutMs={Timeout} path={Path}",
                    "EnumeratePortalItemsAsync",
                    space.Id,
                    timeoutMs,
                    currentPath);
                items = CreateUnavailablePlaceholder(currentPath, "Portal load timed out");
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(
                ex,
                "Portal operation failed: {Operation} space={SpaceId} path={Path}",
                "EnumeratePortalItemsAsync",
                space.Id,
                currentPath);
            items = CreateUnavailablePlaceholder(currentPath, "Folder unavailable");
        }

        lock (_sync)
        {
            _cachedItems[space.Id] = new PortalCacheEntry
            {
                Path = currentPath,
                Items = items,
            };
            _networkLoadsInFlight.Remove(space.Id);
        }

        OnFileChanged(space.Id);
    }

    private static List<SpaceItemModel> EnumeratePortalItemsCore(SpaceModel space, string currentPath)
    {
        var result = new List<SpaceItemModel>();

        // Step 31: Add parent folder marker if not at root
        var portalRoot = new DirectoryInfo(space.PortalFolderPath!);
        var currentDir = new DirectoryInfo(currentPath);
        if (currentDir.FullName != portalRoot.FullName)
        {
            result.Add(new SpaceItemModel
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
            result.Add(new SpaceItemModel
            {
                DisplayName = dirInfo.Name,
                TargetPath = dir,
                IsDirectory = true
            });
        }

        // Step 31: Add files
        foreach (var file in Directory.EnumerateFiles(currentPath))
        {
            result.Add(new SpaceItemModel
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
        Guid spaceId,
        string targetPath)
    {
        try
        {
            var task = Task.Run(operation);
            if (task.Wait(timeoutMs))
                return task.Result;

            Serilog.Log.Warning(
                "Portal operation timed out: {Operation} space={SpaceId} timeoutMs={Timeout} path={Path}",
                operationName,
                spaceId,
                timeoutMs,
                targetPath);
            return fallbackValue;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(
                ex,
                "Portal operation failed: {Operation} space={SpaceId} path={Path}",
                operationName,
                spaceId,
                targetPath);
            return fallbackValue;
        }
    }

    private static List<SpaceItemModel> CreateUnavailablePlaceholder(string path, string reason)
    {
        return new List<SpaceItemModel>
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
