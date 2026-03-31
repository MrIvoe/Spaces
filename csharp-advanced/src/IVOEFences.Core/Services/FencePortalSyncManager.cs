using System.Collections.Concurrent;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Manages FileSystemWatcher integration for portal (folder-based) fences.
/// Keeps fence icons in sync with folder contents in real-time.
/// 
/// Features:
/// - Watches for file creates, deletes, renames
/// - Debounces rapid changes (buffer + async flush)
/// - Automatically adds/removes fence items when folder changes
/// - Thread-safe with lockfree queuing
/// </summary>
public sealed class FencePortalSyncManager : IDisposable
{
    private static readonly Lazy<FencePortalSyncManager> _instance =
        new(() => new FencePortalSyncManager());

    public static FencePortalSyncManager Instance => _instance.Value;

    private readonly ConcurrentDictionary<Guid, FileSystemWatcher> _watchers = new();
    private readonly ConcurrentDictionary<Guid, SyncState> _syncState = new();
    // Single persistent timer — Change() resets the countdown without allocating a new
    // Timer on every file-system event (fixes the dispose-and-recreate race condition).
    private readonly Timer _debounceTimer;

    private const int DebounceDelayMs = 500; // wait this long after last change before syncing

    private class SyncState
    {
        public required FenceModel Fence { get; init; }
        public required string WatchPath { get; init; }
        public HashSet<string> PreviousFiles { get; set; } = new();
        public DateTime LastChangeTime { get; set; } = DateTime.UtcNow;
        public bool IsScheduledForSync { get; set; }
    }

    private FencePortalSyncManager()
    {
        _debounceTimer = new Timer(_ => ProcessScheduledSyncs(), null,
                                   Timeout.Infinite, Timeout.Infinite);
    }

    /// <summary>
    /// Register a fence for portal sync. Starts watching the specified folder.
    /// </summary>
    public void RegisterPortalFence(FenceModel fence, string folderPath)
    {
        if (fence == null || string.IsNullOrEmpty(folderPath))
            return;

        if (!Directory.Exists(folderPath))
        {
            Serilog.Log.Warning("FencePortalSyncManager: Folder does not exist: {Path}", folderPath);
            return;
        }

        // Stop existing watcher if any
        if (_watchers.TryGetValue(fence.Id, out var existingWatcher))
        {
            existingWatcher?.Dispose();
            _watchers.TryRemove(fence.Id, out _);
        }

        // Create new watcher
        var watcher = new FileSystemWatcher(folderPath)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName,
            Filter = "*.*",
            IncludeSubdirectories = false,
        };

        // Event handlers
        watcher.Created += (s, e) => OnFileChanged(fence.Id, e.FullPath);
        watcher.Deleted += (s, e) => OnFileChanged(fence.Id, e.FullPath);
        watcher.Renamed += (s, e) => OnFileChanged(fence.Id, e.FullPath);
        watcher.Error += (s, e) =>
        {
            Serilog.Log.Error("FencePortalSyncManager watch error on {Fence}: {Error}",
                fence.Title, e.GetException()?.Message);
        };

        // Initialize state
        _syncState[fence.Id] = new SyncState
        {
            Fence = fence,
            WatchPath = folderPath,
            PreviousFiles = GetCurrentFiles(folderPath).ToHashSet(),
        };

        // Start watching
        watcher.EnableRaisingEvents = true;
        _watchers[fence.Id] = watcher;

        Serilog.Log.Information("FencePortalSyncManager: Registered portal fence '{Title}' on {Path}",
            fence.Title, folderPath);
    }

    /// <summary>Unregister and stop watching a fence.</summary>
    public void UnregisterPortalFence(Guid fenceId)
    {
        if (_watchers.TryRemove(fenceId, out var watcher))
        {
            watcher?.Dispose();
        }

        _syncState.TryRemove(fenceId, out _);
    }

    /// <summary>Force immediate sync of all watched fences (for testing or manual refresh).</summary>
    public async Task SyncAllAsync()
    {
        var tasks = _syncState.Keys.Select(fenceId => SyncFenceAsync(fenceId)).ToList();
        await Task.WhenAll(tasks);
    }

    /// <summary>Check if a fence is currently being watched.</summary>
    public bool IsPortalFenceWatched(Guid fenceId) =>
        _watchers.ContainsKey(fenceId);

    // ────────────────────────────────────────────────────────────────

    private void OnFileChanged(Guid fenceId, string filePath)
    {
        if (!_syncState.TryGetValue(fenceId, out var state))
            return;

        state.LastChangeTime = DateTime.UtcNow;
        state.IsScheduledForSync = true;

        // Reset the debounce countdown — thread-safe and allocation-free.
        _debounceTimer.Change(DebounceDelayMs, Timeout.Infinite);
    }

    private void ProcessScheduledSyncs()
    {
        _ = Task.Run(async () =>
        {
            foreach (var (fenceId, state) in _syncState)
            {
                if (state.IsScheduledForSync &&
                    DateTime.UtcNow.Subtract(state.LastChangeTime).TotalMilliseconds >= DebounceDelayMs)
                {
                    await SyncFenceAsync(fenceId);
                    state.IsScheduledForSync = false;
                }
            }
        });
    }

    private async Task SyncFenceAsync(Guid fenceId)
    {
        await Task.Run(() =>
        {
            if (!_syncState.TryGetValue(fenceId, out var state))
                return;

            try
            {
                var currentFiles = GetCurrentFiles(state.WatchPath).ToHashSet();
                var previous = state.PreviousFiles;

                // Files deleted
                var deleted = previous.Except(currentFiles).ToList();
                // Files created
                var created = currentFiles.Except(previous).ToList();
                var newItems = new List<FenceItemModel>();
                foreach (var file in created)
                {
                    if (ShouldIncludeInFence(file))
                    {
                        var item = new FenceItemModel
                        {
                            DisplayName = Path.GetFileNameWithoutExtension(file),
                            TargetPath = file,
                            IsDirectory = Directory.Exists(file),
                            IsFromDesktop = false,
                            SortOrder = state.Fence.Items.Count,
                        };
                        FenceUsageTracker.Instance.CacheFileMetadata(item);
                        newItems.Add(item);
                    }
                }

                // Mutate the shared Items list under a lock so the UI thread
                // cannot read a partially-modified list during repaint.
                lock (state.Fence.Items)
                {
                    foreach (var file in deleted)
                        state.Fence.Items.RemoveAll(i => i.TargetPath == file);
                    foreach (var item in newItems)
                        state.Fence.Items.Add(item);
                }

                state.PreviousFiles = currentFiles;
                FenceStateService.Instance.MarkDirty();

                if (deleted.Count > 0 || created.Count > 0)
                {
                    Serilog.Log.Information(
                        "FencePortalSyncManager: Synced '{Fence}' — {Created} added, {Deleted} removed",
                        state.Fence.Title, created.Count, deleted.Count);
                }
            }
            catch (Exception ex)
            {
                Serilog.Log.Error("FencePortalSyncManager: Sync failed for {Fence}: {Error}",
                    state.Fence.Title, ex.Message);
            }
        });
    }

    private static List<string> GetCurrentFiles(string folderPath)
    {
        try
        {
            if (!Directory.Exists(folderPath))
                return new List<string>();

            var files = new List<string>();

            // Include executables
            files.AddRange(Directory.GetFiles(folderPath, "*.exe", SearchOption.TopDirectoryOnly));

            // Include shortcuts
            files.AddRange(Directory.GetFiles(folderPath, "*.lnk", SearchOption.TopDirectoryOnly));

            // Include other common file types (documents, media, etc.)
            // Can be extended based on settings
            var otherExts = new[] { "*.txt", "*.pdf", "*.jpg", "*.png", "*.zip", "*.mp3" };
            foreach (var ext in otherExts)
            {
                files.AddRange(Directory.GetFiles(folderPath, ext, SearchOption.TopDirectoryOnly));
            }

            // Include subdirectories
            files.AddRange(Directory.GetDirectories(folderPath, "*", SearchOption.TopDirectoryOnly));

            return files.Distinct().ToList();
        }
        catch
        {
            return new List<string>();
        }
    }

    private static bool ShouldIncludeInFence(string filePath)
    {
        // Exclude system/hidden files — guard against the file being deleted between
        // the directory scan and this attribute check.
        try
        {
            var attr = File.GetAttributes(filePath);
            if ((attr & FileAttributes.Hidden) != 0 || (attr & FileAttributes.System) != 0)
                return false;
        }
        catch
        {
            return false; // file disappeared
        }

        // Exclude temp/cache files
        var name = Path.GetFileName(filePath).ToLowerInvariant();
        if (name.StartsWith("~") || name.StartsWith("."))
            return false;

        return true;
    }

    public void Dispose()
    {
        _debounceTimer.Dispose();
        foreach (var watcher in _watchers.Values)
        {
            watcher?.Dispose();
        }

        _watchers.Clear();
        _syncState.Clear();
    }
}
