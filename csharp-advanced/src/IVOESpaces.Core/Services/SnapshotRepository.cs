using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 33: Snapshot management for saving and restoring entire space layouts.
/// Snapshots capture all space positions, sizes, items, and state at a point in time.
/// Support for manual save/load, auto-save, and snapshot browsing.
/// </summary>
public sealed class SnapshotRepository
{
    private static SnapshotRepository? _instance;
    private static readonly object _lock = new();

    private readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    private readonly string _snapshotDir;

    public static SnapshotRepository Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new SnapshotRepository();
                }
            }
            return _instance;
        }
    }

    private SnapshotRepository()
    {
        var baseDir = Path.GetDirectoryName(AppPaths.SpacesConfig) ?? Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        _snapshotDir = Path.Combine(baseDir, "snapshots");
        Directory.CreateDirectory(_snapshotDir);
    }

    /// <summary>
    /// Represents a saved snapshot of space layouts.
    /// </summary>
    public sealed record Snapshot(
        Guid Id,
        string Name,
        string? Description,
        DateTime CreatedAt,
        DateTime LastModifiedAt,
        List<SpaceModel> Spaces,
        bool IsAutoBackup)
    {
        public Snapshot() : this(Guid.NewGuid(), string.Empty, null, DateTime.UtcNow, DateTime.UtcNow, new(), false) { }

        public int SpaceCount => Spaces.Count;
        public int TotalItems => Spaces.Sum(f => f.Items?.Count ?? 0);
    }

    /// <summary>
    /// Step 33: Create and save a new snapshot.
    /// </summary>
    public Snapshot CreateSnapshot(string name, string? description = null)
    {
        try
        {
            var spaces = SpaceStateService.Instance.Spaces.ToList();
            var snapshot = new Snapshot(
                Id: Guid.NewGuid(),
                Name: name,
                Description: description,
                CreatedAt: DateTime.UtcNow,
                LastModifiedAt: DateTime.UtcNow,
                Spaces: spaces,
                IsAutoBackup: false
            );

            SaveSnapshotToFile(snapshot);
            Serilog.Log.Information("Snapshot created: {SnapshotName} ({SpaceCount} spaces, {ItemCount} items)",
                name, snapshot.SpaceCount, snapshot.TotalItems);

            return snapshot;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to create snapshot {SnapshotName}", name);
            throw;
        }
    }

    /// <summary>
    /// Step 33: Create an automatic backup snapshot (hidden from user by default).
    /// </summary>
    public Snapshot CreateAutoBackup()
    {
        var timestamp = DateTime.UtcNow.ToString("yyyy-MM-dd_HH-mm-ss");
        var snapshot = new Snapshot(
            Id: Guid.NewGuid(),
            Name: $"auto_backup_{timestamp}",
            Description: "Automatic backup",
            CreatedAt: DateTime.UtcNow,
            LastModifiedAt: DateTime.UtcNow,
            Spaces: SpaceStateService.Instance.Spaces.ToList(),
            IsAutoBackup: true
        );

        SaveSnapshotToFile(snapshot);
        return snapshot;
    }

    /// <summary>
    /// Step 33: Restore spaces from a snapshot.
    /// </summary>
    public async Task<bool> RestoreSnapshot(Guid snapshotId)
    {
        try
        {
            var snapshot = LoadSnapshotFromFile(snapshotId);
            if (snapshot == null)
                return false;

            await SpaceStateService.Instance.ReplaceAllAsync(snapshot.Spaces).ConfigureAwait(false);
            Serilog.Log.Information("Snapshot restored: {SnapshotName} ({SpaceCount} spaces)",
                snapshot.Name, snapshot.SpaceCount);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to restore snapshot {SnapshotId}", snapshotId);
            return false;
        }
    }

    /// <summary>
    /// Step 33: Get list of all saved snapshots.
    /// </summary>
    public List<Snapshot> GetAllSnapshots(bool includeAutoBackups = false)
    {
        var result = new List<Snapshot>();

        try
        {
            if (!Directory.Exists(_snapshotDir))
                return result;

            foreach (var file in Directory.GetFiles(_snapshotDir, "snapshot_*.json"))
            {
                try
                {
                    var snapshot = LoadSnapshotFromFile(file);
                    if (snapshot != null && (includeAutoBackups || !snapshot.IsAutoBackup))
                        result.Add(snapshot);
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "Failed to load snapshot file: {File}", file);
                }
            }

            // Sort by creation date descending (newest first)
            result.Sort((a, b) => b.CreatedAt.CompareTo(a.CreatedAt));
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to enumerate snapshots");
        }

        return result;
    }

    /// <summary>
    /// Step 33: Delete a snapshot by ID.
    /// </summary>
    public bool DeleteSnapshot(Guid snapshotId)
    {
        try
        {
            var path = GetSnapshotFilePath(snapshotId);
            if (File.Exists(path))
            {
                File.Delete(path);
                Serilog.Log.Information("Snapshot deleted: {SnapshotId}", snapshotId);
                return true;
            }
            return false;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to delete snapshot {SnapshotId}", snapshotId);
            return false;
        }
    }

    /// <summary>
    /// Step 33: Rename a snapshot.
    /// </summary>
    public bool RenameSnapshot(Guid snapshotId, string newName)
    {
        try
        {
            var snapshot = LoadSnapshotFromFile(snapshotId);
            if (snapshot == null)
                return false;

            var renamed = snapshot with { Name = newName, LastModifiedAt = DateTime.UtcNow };

            SaveSnapshotToFile(renamed);
            Serilog.Log.Information("Snapshot renamed: {SnapshotId} → {NewName}", snapshotId, newName);
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to rename snapshot {SnapshotId}", snapshotId);
            return false;
        }
    }

    /// <summary>
    /// Step 33: Export snapshot as portable JSON file.
    /// </summary>
    public bool ExportSnapshot(Guid snapshotId, string exportPath)
    {
        try
        {
            var snapshot = LoadSnapshotFromFile(snapshotId);
            if (snapshot == null)
                return false;

            string json = JsonSerializer.Serialize(snapshot, _jsonOptions);
            File.WriteAllText(exportPath, json);
            Serilog.Log.Information("Snapshot exported: {SnapshotId} → {ExportPath}", snapshotId, exportPath);
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to export snapshot {SnapshotId}", snapshotId);
            return false;
        }
    }

    /// <summary>
    /// Step 33: Import snapshot from exported JSON file.
    /// </summary>
    public Snapshot? ImportSnapshot(string importPath, string? overrideName = null)
    {
        try
        {
            if (!File.Exists(importPath))
                return null;

            string json = File.ReadAllText(importPath);
            var imported = JsonSerializer.Deserialize<Snapshot>(json, _jsonOptions);

            if (imported == null)
                return null;

            // Generate new ID and optionally override name
            var snapshot = imported with
            {
                Id = Guid.NewGuid(),
                Name = overrideName ?? imported.Name,
                CreatedAt = DateTime.UtcNow,
                LastModifiedAt = DateTime.UtcNow,
                IsAutoBackup = false
            };

            SaveSnapshotToFile(snapshot);
            Serilog.Log.Information("Snapshot imported: {ImportPath} as {SnapshotName}", 
                importPath, snapshot.Name);

            return snapshot;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to import snapshot from {ImportPath}", importPath);
            return null;
        }
    }

    /// <summary>
    /// Step 33: Auto-cleanup old backup snapshots (keep last N).
    /// </summary>
    public int CleanupOldBackups(int keepCount = 5)
    {
        try
        {
            var backups = GetAllSnapshots(includeAutoBackups: true)
                .Where(s => s.IsAutoBackup)
                .OrderByDescending(s => s.CreatedAt)
                .ToList();

            int deleted = 0;
            for (int i = keepCount; i < backups.Count; i++)
            {
                if (DeleteSnapshot(backups[i].Id))
                    deleted++;
            }

            Serilog.Log.Information("Cleanup complete: {DeletedCount} old backups removed", deleted);
            return deleted;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to cleanup old backups");
            return 0;
        }
    }

    // ── PRIVATE HELPERS ──

    private string GetSnapshotFilePath(Guid snapshotId) =>
        Path.Combine(_snapshotDir, $"snapshot_{snapshotId:N}.json");

    private void SaveSnapshotToFile(Snapshot snapshot)
    {
        var path = GetSnapshotFilePath(snapshot.Id);
        string json = JsonSerializer.Serialize(snapshot, _jsonOptions);
        File.WriteAllText(path, json);
    }

    private Snapshot? LoadSnapshotFromFile(Guid snapshotId)
    {
        var path = GetSnapshotFilePath(snapshotId);
        return LoadSnapshotFromFile(path);
    }

    private Snapshot? LoadSnapshotFromFile(string filePath)
    {
        try
        {
            if (!File.Exists(filePath))
                return null;

            string json = File.ReadAllText(filePath);
            return JsonSerializer.Deserialize<Snapshot>(json, _jsonOptions);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Failed to load snapshot from {File}", filePath);
            return null;
        }
    }
}
