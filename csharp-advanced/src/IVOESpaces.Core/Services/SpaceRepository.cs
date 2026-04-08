using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Text.Json;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public class SpaceRepository : ISpaceRepository
{
    private static readonly Lazy<SpaceRepository> _instance = new(() => new SpaceRepository());
    public static SpaceRepository Instance => _instance.Value;

    private readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    private readonly SemaphoreSlim _fileLock = new(1, 1);
    private readonly string _spacesConfigPath;
    private List<SpaceModel> _currentSpaces = new();

    private string SpacesBackup1Path => _spacesConfigPath + ".bak";
    private string SpacesBackup2Path => _spacesConfigPath + ".bak2";
    private string SpacesBackup3Path => _spacesConfigPath + ".bak3";

    public SpaceRepository() : this(AppPaths.SpacesConfig)
    {
    }

    public SpaceRepository(string spacesConfigPath)
    {
        _spacesConfigPath = spacesConfigPath;
    }

    public static SpaceRepository CreateForTesting(string spacesConfigPath)
    {
        return new SpaceRepository(spacesConfigPath);
    }

    public async Task<List<SpaceModel>> LoadAllAsync()
    {
        // Track whether a legacy-format file needs migration. Migration must happen
        // OUTSIDE the _fileLock scope because MigrateLegacyJsonIfNeeded calls
        // SaveAllAsync → PersistAsync which tries to re-acquire _fileLock (deadlock).
        string? pathToMigrate = null;

        bool loaded = false;

        await _fileLock.WaitAsync();
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.SpacesConfig) ?? ".");
            string[] candidates = { _spacesConfigPath, SpacesBackup1Path, SpacesBackup2Path, SpacesBackup3Path };
            foreach (var path in candidates)
            {
                if (!File.Exists(path))
                    continue;

                try
                {
                    string json = await File.ReadAllTextAsync(path);
                    var spaces = JsonSerializer.Deserialize<List<SpaceModel>>(json, _jsonOptions);
                    if (spaces is not null)
                    {
                        _currentSpaces = spaces;
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
                _currentSpaces = new List<SpaceModel>();
        }
        finally
        {
            _fileLock.Release();
        }

        // Migration is safe here: _fileLock is released so PersistAsync can acquire it.
        if (pathToMigrate != null)
            await MigrateLegacyJsonIfNeeded(pathToMigrate);

        return _currentSpaces;
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

                var migrated = new List<SpaceModel>();
                foreach (var space in legacy)
                {
                    var model = new SpaceModel
                    {
                        Id = space.ContainsKey("Id") && Guid.TryParse(space["Id"].ToString(), out var g) ? g : Guid.NewGuid(),
                        Title = space.TryGetValue("Title", out var t) ? t?.ToString() ?? "New Space" : "New Space"
                    };

                    if (space.TryGetValue("X", out var vx) && space.TryGetValue("Y", out var vy) && space.TryGetValue("Width", out var vw) && space.TryGetValue("Height", out var vh))
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

                _currentSpaces = migrated;
                await SaveAllAsync(_currentSpaces);
            }
            catch
            {
                // ignore migration failure
            }
        }
    }

    public async Task SaveAllAsync(IEnumerable<SpaceModel> spaces)
    {
        _currentSpaces = spaces.ToList();
        await PersistAsync(_currentSpaces);
    }

    public async Task<SpaceModel> SaveOneAsync(SpaceModel space)
    {
        var existing = _currentSpaces.FirstOrDefault(f => f.Id == space.Id);
        if (existing is not null)
        {
            _currentSpaces.Remove(existing);
        }

        _currentSpaces.Add(space);
        await PersistAsync(_currentSpaces);
        return space;
    }

    public async Task DeleteAsync(Guid spaceId)
    {
        _currentSpaces.RemoveAll(f => f.Id == spaceId);
        await PersistAsync(_currentSpaces);
    }

    private async Task PersistAsync(IEnumerable<SpaceModel> spaces)
    {
        await _fileLock.WaitAsync();
        try
        {
            var directory = Path.GetDirectoryName(AppPaths.SpacesConfig) ?? ".";
            directory = Path.GetDirectoryName(_spacesConfigPath) ?? ".";
            Directory.CreateDirectory(directory);

            string tempFile = Path.Combine(directory, Path.GetFileName(_spacesConfigPath) + ".tmp");
            string json = JsonSerializer.Serialize(spaces, _jsonOptions);
            await File.WriteAllTextAsync(tempFile, json);

            if (File.Exists(_spacesConfigPath))
            {
                RotateSpaceBackups();
                File.Replace(tempFile, _spacesConfigPath, SpacesBackup1Path);
            }
            else
            {
                File.Move(tempFile, _spacesConfigPath);
            }
        }
        finally
        {
            _fileLock.Release();
        }
    }

    private void RotateSpaceBackups()
    {
        if (File.Exists(SpacesBackup2Path))
            File.Copy(SpacesBackup2Path, SpacesBackup3Path, overwrite: true);

        if (File.Exists(SpacesBackup1Path))
            File.Copy(SpacesBackup1Path, SpacesBackup2Path, overwrite: true);
    }

    // Multi-monitor configuration layout persistence
    private string GetLayoutSnapshotPath(string configHash) =>
        Path.Combine(Path.GetDirectoryName(_spacesConfigPath) ?? ".", 
            $"layouts", $"config_{configHash}.json");

    public async Task SaveLayoutSnapshotAsync(string configHash, List<SpaceModel> spaces)
    {
        await _fileLock.WaitAsync();
        try
        {
            var layoutPath = GetLayoutSnapshotPath(configHash);
            Directory.CreateDirectory(Path.GetDirectoryName(layoutPath) ?? ".");
            
            string json = JsonSerializer.Serialize(new
            {
                ConfigHash = configHash,
                Spaces = spaces,
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

    public async Task<List<SpaceModel>> LoadLayoutSnapshotAsync(string configHash)
    {
        await _fileLock.WaitAsync();
        try
        {
            var layoutPath = GetLayoutSnapshotPath(configHash);
            if (!File.Exists(layoutPath))
                return new List<SpaceModel>();

            string json = await File.ReadAllTextAsync(layoutPath);
            var data = JsonSerializer.Deserialize<JsonElement>(json, _jsonOptions);
            
            if (data.TryGetProperty("Spaces", out var spacesElement))
            {
                var spaces = JsonSerializer.Deserialize<List<SpaceModel>>(
                    spacesElement.GetRawText(), _jsonOptions);
                return spaces ?? new List<SpaceModel>();
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

        return new List<SpaceModel>();
    }

    public List<(string Hash, int SpaceCount, DateTime LastActive)> GetAllSavedConfigurations()
    {
        var result = new List<(string, int, DateTime)>();
        
        try
        {
            var layoutDir = Path.Combine(Path.GetDirectoryName(AppPaths.SpacesConfig) ?? ".", "layouts");
            if (!Directory.Exists(layoutDir))
                return result;

            foreach (var file in Directory.GetFiles(layoutDir, "config_*.json"))
            {
                try
                {
                    string json = File.ReadAllText(file);
                    var data = JsonSerializer.Deserialize<JsonElement>(json, _jsonOptions);
                    
                    if (data.TryGetProperty("ConfigHash", out var hashElem) &&
                        data.TryGetProperty("Spaces", out var spacesElem) &&
                        data.TryGetProperty("SavedAt", out var dateElem))
                    {
                        var hash = hashElem.GetString() ?? "unknown";
                        var spaceCount = spacesElem.GetArrayLength();
                        var date = dateElem.GetDateTime();
                        
                        result.Add((hash, spaceCount, date));
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
        Path.Combine(Path.GetDirectoryName(_spacesConfigPath) ?? ".",
            "tab_containers.json");

    /// <summary>
    /// Step 35: Load all tab containers from persistent storage.
    /// </summary>
    public async Task<List<SpaceTabModel>> LoadTabContainersAsync()
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
                    var containers = JsonSerializer.Deserialize<List<SpaceTabModel>>(json, _jsonOptions);
                    if (containers != null)
                        return containers;
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "Failed to load tab containers candidate {Path}", candidate);
                }
            }

            return new List<SpaceTabModel>();
        }
        finally
        {
            _fileLock.Release();
        }
    }

    /// <summary>
    /// Step 35: Save all tab containers to persistent storage.
    /// </summary>
    public async Task SaveTabContainersAsync(IEnumerable<SpaceTabModel> containers)
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
    public List<SpaceTabModel> LoadTabContainers()
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
                    var containers = JsonSerializer.Deserialize<List<SpaceTabModel>>(json, _jsonOptions);
                    if (containers != null)
                        return containers;
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "Failed to load tab containers candidate {Path}", candidate);
                }
            }

            return new List<SpaceTabModel>();
        }
        finally
        {
            _fileLock.Release();
        }
    }

    /// <summary>
    /// Step 35: Synchronous save path for tab containers.
    /// </summary>
    public void SaveTabContainers(IEnumerable<SpaceTabModel> containers)
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
