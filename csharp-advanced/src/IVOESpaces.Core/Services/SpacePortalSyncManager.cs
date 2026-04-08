using System.Collections.Concurrent;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Manages FileSystemWatcher integration for portal (folder-based) spaces.
/// Keeps space icons in sync with folder contents in real-time.
/// 
/// Features:
/// - Watches for file creates, deletes, renames
/// - Debounces rapid changes (buffer + async flush)
/// - Automatically adds/removes space items when folder changes
/// - Thread-safe with lockfree queuing
/// </summary>
public sealed class SpacePortalSyncManager : IDisposable
{
    private static readonly Lazy<SpacePortalSyncManager> _instance =
        new(() => new SpacePortalSyncManager());

    public static SpacePortalSyncManager Instance => _instance.Value;

    private readonly ConcurrentDictionary<Guid, FileSystemWatcher> _watchers = new();
    private readonly ConcurrentDictionary<Guid, SyncState> _syncState = new();
    // Single persistent timer — Change() resets the countdown without allocating a new
    // Timer on every file-system event (fixes the dispose-and-recreate race condition).
    private readonly Timer _debounceTimer;

    private const int DebounceDelayMs = 500; // wait this long after last change before syncing

    private class SyncState
    {
        public required SpaceModel Space { get; init; }
        public required string WatchPath { get; init; }
        public HashSet<string> PreviousFiles { get; set; } = new();
        public DateTime LastChangeTime { get; set; } = DateTime.UtcNow;
        public bool IsScheduledForSync { get; set; }
    }

    private SpacePortalSyncManager()
    {
        _debounceTimer = new Timer(_ => ProcessScheduledSyncs(), null,
                                   Timeout.Infinite, Timeout.Infinite);
    }

    /// <summary>
    /// Register a space for portal sync. Starts watching the specified folder.
    /// </summary>
    public void RegisterPortalSpace(SpaceModel space, string folderPath)
    {
        if (space == null || string.IsNullOrEmpty(folderPath))
            return;

        if (!Directory.Exists(folderPath))
        {
            Serilog.Log.Warning("SpacePortalSyncManager: Folder does not exist: {Path}", folderPath);
            return;
        }

        // Stop existing watcher if any
        if (_watchers.TryGetValue(space.Id, out var existingWatcher))
        {
            existingWatcher?.Dispose();
            _watchers.TryRemove(space.Id, out _);
        }

        // Create new watcher
        var watcher = new FileSystemWatcher(folderPath)
        {
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName,
            Filter = "*.*",
            IncludeSubdirectories = false,
        };

        // Event handlers
        watcher.Created += (s, e) => OnFileChanged(space.Id, e.FullPath);
        watcher.Deleted += (s, e) => OnFileChanged(space.Id, e.FullPath);
        watcher.Renamed += (s, e) => OnFileChanged(space.Id, e.FullPath);
        watcher.Error += (s, e) =>
        {
            Serilog.Log.Error("SpacePortalSyncManager watch error on {Space}: {Error}",
                space.Title, e.GetException()?.Message);
        };

        // Initialize state
        _syncState[space.Id] = new SyncState
        {
            Space = space,
            WatchPath = folderPath,
            PreviousFiles = GetCurrentFiles(folderPath).ToHashSet(),
        };

        // Start watching
        watcher.EnableRaisingEvents = true;
        _watchers[space.Id] = watcher;

        Serilog.Log.Information("SpacePortalSyncManager: Registered portal space '{Title}' on {Path}",
            space.Title, folderPath);
    }

    /// <summary>Unregister and stop watching a space.</summary>
    public void UnregisterPortalSpace(Guid spaceId)
    {
        if (_watchers.TryRemove(spaceId, out var watcher))
        {
            watcher?.Dispose();
        }

        _syncState.TryRemove(spaceId, out _);
    }

    /// <summary>Force immediate sync of all watched spaces (for testing or manual refresh).</summary>
    public async Task SyncAllAsync()
    {
        var tasks = _syncState.Keys.Select(spaceId => SyncSpaceAsync(spaceId)).ToList();
        await Task.WhenAll(tasks);
    }

    /// <summary>Check if a space is currently being watched.</summary>
    public bool IsPortalSpaceWatched(Guid spaceId) =>
        _watchers.ContainsKey(spaceId);

    // ────────────────────────────────────────────────────────────────

    private void OnFileChanged(Guid spaceId, string filePath)
    {
        if (!_syncState.TryGetValue(spaceId, out var state))
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
            foreach (var (spaceId, state) in _syncState)
            {
                if (state.IsScheduledForSync &&
                    DateTime.UtcNow.Subtract(state.LastChangeTime).TotalMilliseconds >= DebounceDelayMs)
                {
                    await SyncSpaceAsync(spaceId);
                    state.IsScheduledForSync = false;
                }
            }
        });
    }

    private async Task SyncSpaceAsync(Guid spaceId)
    {
        await Task.Run(() =>
        {
            if (!_syncState.TryGetValue(spaceId, out var state))
                return;

            try
            {
                var currentFiles = GetCurrentFiles(state.WatchPath).ToHashSet();
                var previous = state.PreviousFiles;

                // Files deleted
                var deleted = previous.Except(currentFiles).ToList();
                // Files created
                var created = currentFiles.Except(previous).ToList();
                var newItems = new List<SpaceItemModel>();
                foreach (var file in created)
                {
                    if (ShouldIncludeInSpace(file))
                    {
                        var item = new SpaceItemModel
                        {
                            DisplayName = Path.GetFileNameWithoutExtension(file),
                            TargetPath = file,
                            IsDirectory = Directory.Exists(file),
                            IsFromDesktop = false,
                            SortOrder = state.Space.Items.Count,
                        };
                        SpaceUsageTracker.Instance.CacheFileMetadata(item);
                        newItems.Add(item);
                    }
                }

                // Mutate the shared Items list under a lock so the UI thread
                // cannot read a partially-modified list during repaint.
                lock (state.Space.Items)
                {
                    foreach (var file in deleted)
                        state.Space.Items.RemoveAll(i => i.TargetPath == file);
                    foreach (var item in newItems)
                        state.Space.Items.Add(item);
                }

                state.PreviousFiles = currentFiles;
                SpaceStateService.Instance.MarkDirty();

                if (deleted.Count > 0 || created.Count > 0)
                {
                    Serilog.Log.Information(
                        "SpacePortalSyncManager: Synced '{Space}' — {Created} added, {Deleted} removed",
                        state.Space.Title, created.Count, deleted.Count);
                }
            }
            catch (Exception ex)
            {
                Serilog.Log.Error("SpacePortalSyncManager: Sync failed for {Space}: {Error}",
                    state.Space.Title, ex.Message);
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

    private static bool ShouldIncludeInSpace(string filePath)
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
