using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Text.Json;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public class FenceRepository : IFenceRepository
{
    private static readonly Lazy<FenceRepository> _instance = new(() => new FenceRepository());
    public static FenceRepository Instance => _instance.Value;

    private readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    private readonly SemaphoreSlim _fileLock = new(1, 1);
    private readonly string _fencesConfigPath;
    private List<FenceModel> _currentFences = new();

    private string FencesBackup1Path => _fencesConfigPath + ".bak";
    private string FencesBackup2Path => _fencesConfigPath + ".bak2";
    private string FencesBackup3Path => _fencesConfigPath + ".bak3";

    public FenceRepository() : this(AppPaths.FencesConfig)
    {
    }

    public FenceRepository(string fencesConfigPath)
    {
        _fencesConfigPath = fencesConfigPath;
    }

    public static FenceRepository CreateForTesting(string fencesConfigPath)
    {
        return new FenceRepository(fencesConfigPath);
    }

    public async Task<List<FenceModel>> LoadAllAsync()
    {
        // Track whether a legacy-format file needs migration. Migration must happen
        // OUTSIDE the _fileLock scope because MigrateLegacyJsonIfNeeded calls
        // SaveAllAsync → PersistAsync which tries to re-acquire _fileLock (deadlock).
        string? pathToMigrate = null;

        bool loaded = false;

        await _fileLock.WaitAsync();
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.FencesConfig) ?? ".");
            string[] candidates = { _fencesConfigPath, FencesBackup1Path, FencesBackup2Path, FencesBackup3Path };
            foreach (var path in candidates)
            {
                if (!File.Exists(path))
                    continue;

                try
                {
                    string json = await File.ReadAllTextAsync(path);
                    var fences = JsonSerializer.Deserialize<List<FenceModel>>(json, _jsonOptions);
                    if (fences is not null)
                    {
                        _currentFences = fences;
                        loaded = true;
                        // Detect legacy format — migrate after the lock is released.
                        if (json.Contains("\"X\"", StringComparison.Ordinal)
                            && json.Contains("\"Y\"", StringComparison.Ordinal))
                            pathToMigrate = path;
                        break;
                    }
                }
                catch
                {
                    // try next candidate
                }
            }

            if (!loaded)
                _currentFences = new List<FenceModel>();
        }
        finally
        {
            _fileLock.Release();
        }

        // Migration is safe here: _fileLock is released so PersistAsync can acquire it.
        if (pathToMigrate != null)
            await MigrateLegacyJsonIfNeeded(pathToMigrate);

        return _currentFences;
    }

    private async Task MigrateLegacyJsonIfNeeded(string configPath)
    {
        string json = await File.ReadAllTextAsync(configPath);
        if (json.Contains("\"X\"", StringComparison.Ordinal) && json.Contains("\"Y\"", StringComparison.Ordinal))
        {
            try
            {
                var legacy = JsonSerializer.Deserialize<List<Dictionary<string, object>>>(json, _jsonOptions);
                if (legacy is null)
                    return;

                var migrated = new List<FenceModel>();
                foreach (var fence in legacy)
                {
                    var model = new FenceModel
                    {
                        Id = fence.ContainsKey("Id") && Guid.TryParse(fence["Id"].ToString(), out var g) ? g : Guid.NewGuid(),
                        Title = fence.TryGetValue("Title", out var t) ? t?.ToString() ?? "New Fence" : "New Fence"
                    };

                    if (fence.TryGetValue("X", out var vx) && fence.TryGetValue("Y", out var vy) && fence.TryGetValue("Width", out var vw) && fence.TryGetValue("Height", out var vh))
                    {
                        if (double.TryParse(vx?.ToString(), out var x) && double.TryParse(vy?.ToString(), out var y) &&
                            double.TryParse(vw?.ToString(), out var w) && double.TryParse(vh?.ToString(), out var h))
                        {
                            model.XFraction = x;
                            model.YFraction = y;
                            model.WidthFraction = w;
                            model.HeightFraction = h;
                        }
                    }

                    migrated.Add(model);
                }

                _currentFences = migrated;
                await SaveAllAsync(_currentFences);
            }
            catch
            {
                // ignore migration failure
            }
        }
    }

    public async Task SaveAllAsync(IEnumerable<FenceModel> fences)
    {
        _currentFences = fences.ToList();
        await PersistAsync(_currentFences);
    }

    public async Task<FenceModel> SaveOneAsync(FenceModel fence)
    {
        var existing = _currentFences.FirstOrDefault(f => f.Id == fence.Id);
        if (existing is not null)
        {
            _currentFences.Remove(existing);
        }

        _currentFences.Add(fence);
        await PersistAsync(_currentFences);
        return fence;
    }

    public async Task DeleteAsync(Guid fenceId)
    {
        _currentFences.RemoveAll(f => f.Id == fenceId);
        await PersistAsync(_currentFences);
    }

    private async Task PersistAsync(IEnumerable<FenceModel> fences)
    {
        await _fileLock.WaitAsync();
        try
        {
            var directory = Path.GetDirectoryName(AppPaths.FencesConfig) ?? ".";
            directory = Path.GetDirectoryName(_fencesConfigPath) ?? ".";
            Directory.CreateDirectory(directory);

            string tempFile = Path.Combine(directory, Path.GetFileName(_fencesConfigPath) + ".tmp");
            string json = JsonSerializer.Serialize(fences, _jsonOptions);
            await File.WriteAllTextAsync(tempFile, json);

            if (File.Exists(_fencesConfigPath))
            {
                RotateFenceBackups();
                File.Replace(tempFile, _fencesConfigPath, FencesBackup1Path);
            }
            else
            {
                File.Move(tempFile, _fencesConfigPath);
            }
        }
        finally
        {
            _fileLock.Release();
        }
    }

    private void RotateFenceBackups()
    {
        if (File.Exists(FencesBackup2Path))
            File.Copy(FencesBackup2Path, FencesBackup3Path, overwrite: true);

        if (File.Exists(FencesBackup1Path))
            File.Copy(FencesBackup1Path, FencesBackup2Path, overwrite: true);
    }

    // Multi-monitor configuration layout persistence
    private string GetLayoutSnapshotPath(string configHash) =>
        Path.Combine(Path.GetDirectoryName(_fencesConfigPath) ?? ".", 
            $"layouts", $"config_{configHash}.json");

    public async Task SaveLayoutSnapshotAsync(string configHash, List<FenceModel> fences)
    {
        await _fileLock.WaitAsync();
        try
        {
            var layoutPath = GetLayoutSnapshotPath(configHash);
            Directory.CreateDirectory(Path.GetDirectoryName(layoutPath) ?? ".");
            
            string json = JsonSerializer.Serialize(new
            {
                ConfigHash = configHash,
                Fences = fences,
                SavedAt = DateTime.UtcNow
            }, _jsonOptions);
            
            string tempFile = layoutPath + ".tmp";
            await File.WriteAllTextAsync(tempFile, json);
            File.Move(tempFile, layoutPath, overwrite: true);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to save layout snapshot for config {Hash}", configHash);
        }
        finally
        {
            _fileLock.Release();
        }
    }

    public async Task<List<FenceModel>> LoadLayoutSnapshotAsync(string configHash)
    {
        await _fileLock.WaitAsync();
        try
        {
            var layoutPath = GetLayoutSnapshotPath(configHash);
            if (!File.Exists(layoutPath))
                return new List<FenceModel>();

            string json = await File.ReadAllTextAsync(layoutPath);
            var data = JsonSerializer.Deserialize<JsonElement>(json, _jsonOptions);
            
            if (data.TryGetProperty("Fences", out var fencesElement))
            {
                var fences = JsonSerializer.Deserialize<List<FenceModel>>(
                    fencesElement.GetRawText(), _jsonOptions);
                return fences ?? new List<FenceModel>();
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to load layout snapshot for config {Hash}", configHash);
        }
        finally
        {
            _fileLock.Release();
        }

        return new List<FenceModel>();
    }

    public List<(string Hash, int FenceCount, DateTime LastActive)> GetAllSavedConfigurations()
    {
        var result = new List<(string, int, DateTime)>();
        
        try
        {
            var layoutDir = Path.Combine(Path.GetDirectoryName(AppPaths.FencesConfig) ?? ".", "layouts");
            if (!Directory.Exists(layoutDir))
                return result;

            foreach (var file in Directory.GetFiles(layoutDir, "config_*.json"))
            {
                try
                {
                    string json = File.ReadAllText(file);
                    var data = JsonSerializer.Deserialize<JsonElement>(json, _jsonOptions);
                    
                    if (data.TryGetProperty("ConfigHash", out var hashElem) &&
                        data.TryGetProperty("Fences", out var fencesElem) &&
                        data.TryGetProperty("SavedAt", out var dateElem))
                    {
                        var hash = hashElem.GetString() ?? "unknown";
                        var fenceCount = fencesElem.GetArrayLength();
                        var date = dateElem.GetDateTime();
                        
                        result.Add((hash, fenceCount, date));
                    }
                }
                catch
                {
                    // Ignore individual file errors
                }
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to retrieve saved configurations");
        }

        return result;
    }

    // Step 35: Tab Container persistence methods
    private string GetTabContainersPath() =>
        Path.Combine(Path.GetDirectoryName(_fencesConfigPath) ?? ".",
            "tab_containers.json");

    /// <summary>
    /// Step 35: Load all tab containers from persistent storage.
    /// </summary>
    public async Task<List<FenceTabModel>> LoadTabContainersAsync()
    {
        await _fileLock.WaitAsync();
        try
        {
            var path = GetTabContainersPath();
            string[] candidates = { path, path + ".bak", path + ".bak2", path + ".bak3" };
            foreach (string candidate in candidates)
            {
                if (!File.Exists(candidate))
                    continue;

                try
                {
                    string json = await File.ReadAllTextAsync(candidate);
                    var containers = JsonSerializer.Deserialize<List<FenceTabModel>>(json, _jsonOptions);
                    if (containers != null)
                        return containers;
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "Failed to load tab containers candidate {Path}", candidate);
                }
            }

            return new List<FenceTabModel>();
        }
        finally
        {
            _fileLock.Release();
        }
    }

    /// <summary>
    /// Step 35: Save all tab containers to persistent storage.
    /// </summary>
    public async Task SaveTabContainersAsync(IEnumerable<FenceTabModel> containers)
    {
        await _fileLock.WaitAsync();
        try
        {
            var path = GetTabContainersPath();
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? ".");

            try
            {
                if (File.Exists(path + ".bak2"))
                    File.Copy(path + ".bak2", path + ".bak3", overwrite: true);

                if (File.Exists(path + ".bak"))
                    File.Copy(path + ".bak", path + ".bak2", overwrite: true);

                string json = JsonSerializer.Serialize(containers.ToList(), _jsonOptions);
                string tempFile = path + ".tmp";
                await File.WriteAllTextAsync(tempFile, json);

                if (File.Exists(path))
                    File.Replace(tempFile, path, path + ".bak");
                else
                    File.Move(tempFile, path);
            }
            catch (Exception ex)
            {
                Serilog.Log.Error(ex, "Failed to save tab containers to {Path}", path);
            }
        }
        finally
        {
            _fileLock.Release();
        }
    }

    /// <summary>
    /// Step 35: Synchronous load path for tab containers.
    /// </summary>
    public List<FenceTabModel> LoadTabContainers()
    {
        _fileLock.Wait();
        try
        {
            var path = GetTabContainersPath();
            string[] candidates = { path, path + ".bak", path + ".bak2", path + ".bak3" };
            foreach (string candidate in candidates)
            {
                if (!File.Exists(candidate))
                    continue;

                try
                {
                    string json = File.ReadAllText(candidate);
                    var containers = JsonSerializer.Deserialize<List<FenceTabModel>>(json, _jsonOptions);
                    if (containers != null)
                        return containers;
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "Failed to load tab containers candidate {Path}", candidate);
                }
            }

            return new List<FenceTabModel>();
        }
        finally
        {
            _fileLock.Release();
        }
    }

    /// <summary>
    /// Step 35: Synchronous save path for tab containers.
    /// </summary>
    public void SaveTabContainers(IEnumerable<FenceTabModel> containers)
    {
        _fileLock.Wait();
        try
        {
            var path = GetTabContainersPath();
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? ".");

            try
            {
                if (File.Exists(path + ".bak2"))
                    File.Copy(path + ".bak2", path + ".bak3", overwrite: true);

                if (File.Exists(path + ".bak"))
                    File.Copy(path + ".bak", path + ".bak2", overwrite: true);

                string json = JsonSerializer.Serialize(containers.ToList(), _jsonOptions);
                string tempFile = path + ".tmp";
                File.WriteAllText(tempFile, json);

                if (File.Exists(path))
                    File.Replace(tempFile, path, path + ".bak");
                else
                    File.Move(tempFile, path);
            }
            catch (Exception ex)
            {
                Serilog.Log.Error(ex, "Failed to save tab containers to {Path}", path);
            }
        }
        finally
        {
            _fileLock.Release();
        }
    }
}
