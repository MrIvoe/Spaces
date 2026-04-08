using IVOESpaces.Core.Models;
using System.Text.Json;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Registry of durable desktop entities and virtual ownership state.
/// Spaces reference entities; files remain in their native filesystem locations.
/// </summary>
public sealed class DesktopEntityRegistryService
{
    private static readonly Lazy<DesktopEntityRegistryService> _instance = new(() => new DesktopEntityRegistryService());
    public static DesktopEntityRegistryService Instance => _instance.Value;

    private readonly object _lock = new();
    private readonly Dictionary<Guid, DesktopEntityModel> _byId = new();
    private readonly Dictionary<string, Guid> _byPath = new(StringComparer.OrdinalIgnoreCase);
    private readonly string _registryFilePath;

    private DesktopEntityRegistryService() : this(AppPaths.DesktopEntitiesConfig)
    {
    }

    internal DesktopEntityRegistryService(string registryFilePath)
    {
        _registryFilePath = registryFilePath;
        Load();
    }

    public static DesktopEntityRegistryService CreateForTesting(string registryFilePath)
    {
        return new DesktopEntityRegistryService(registryFilePath);
    }

    public DesktopEntityModel EnsureEntity(string path, string? displayName, bool isDirectory)
    {
        string normalizedPath = NormalizePath(path);
        lock (_lock)
        {
            if (_byPath.TryGetValue(normalizedPath, out Guid existingId) && _byId.TryGetValue(existingId, out DesktopEntityModel? existing))
            {
                existing.DisplayName = string.IsNullOrWhiteSpace(displayName) ? existing.DisplayName : displayName;
                existing.FileSystemPath = normalizedPath;
                existing.ParsingName = normalizedPath;
                existing.IsDirectory = isDirectory;
                existing.IsShortcut = string.Equals(Path.GetExtension(normalizedPath), ".lnk", StringComparison.OrdinalIgnoreCase);
                existing.Extension = isDirectory ? null : Path.GetExtension(normalizedPath);
                existing.LastSeenUtc = DateTime.UtcNow;
                SaveLocked();
                return existing;
            }

            var entity = new DesktopEntityModel
            {
                Id = Guid.NewGuid(),
                DisplayName = string.IsNullOrWhiteSpace(displayName)
                    ? (isDirectory ? Path.GetFileName(normalizedPath) : Path.GetFileNameWithoutExtension(normalizedPath))
                    : displayName,
                ParsingName = normalizedPath,
                FileSystemPath = normalizedPath,
                IsDirectory = isDirectory,
                IsShortcut = string.Equals(Path.GetExtension(normalizedPath), ".lnk", StringComparison.OrdinalIgnoreCase),
                Extension = isDirectory ? null : Path.GetExtension(normalizedPath),
                Ownership = DesktopItemOwnership.DesktopOnly,
                LastSeenUtc = DateTime.UtcNow,
            };

            _byId[entity.Id] = entity;
            _byPath[normalizedPath] = entity.Id;
            SaveLocked();
            return entity;
        }
    }

    public DesktopEntityModel? TryGetById(Guid id)
    {
        lock (_lock)
        {
            _byId.TryGetValue(id, out DesktopEntityModel? entity);
            return entity;
        }
    }

    public DesktopEntityModel? TryGetByPath(string path)
    {
        string normalizedPath = NormalizePath(path);
        lock (_lock)
        {
            if (_byPath.TryGetValue(normalizedPath, out Guid id) && _byId.TryGetValue(id, out DesktopEntityModel? entity))
                return entity;
            return null;
        }
    }

    public void AssignToSpace(Guid entityId, Guid spaceId)
    {
        lock (_lock)
        {
            if (!_byId.TryGetValue(entityId, out DesktopEntityModel? entity))
                return;

            entity.Ownership = DesktopItemOwnership.SpaceManaged;
            entity.OwnerSpaceId = spaceId;
            entity.LastSeenUtc = DateTime.UtcNow;
            SaveLocked();
        }
    }

    public void ReturnToDesktop(Guid entityId)
    {
        lock (_lock)
        {
            if (!_byId.TryGetValue(entityId, out DesktopEntityModel? entity))
                return;

            entity.Ownership = DesktopItemOwnership.DesktopOnly;
            entity.OwnerSpaceId = null;
            entity.LastSeenUtc = DateTime.UtcNow;
            SaveLocked();
        }
    }

    public void HandleRename(string oldPath, string newPath, string? newDisplayName = null)
    {
        string oldNormalized = NormalizePath(oldPath);
        string newNormalized = NormalizePath(newPath);

        lock (_lock)
        {
            if (!_byPath.TryGetValue(oldNormalized, out Guid id) || !_byId.TryGetValue(id, out DesktopEntityModel? entity))
                return;

            _byPath.Remove(oldNormalized);
            _byPath[newNormalized] = id;

            entity.FileSystemPath = newNormalized;
            entity.ParsingName = newNormalized;
            if (!string.IsNullOrWhiteSpace(newDisplayName))
                entity.DisplayName = newDisplayName;
            entity.IsDirectory = Directory.Exists(newNormalized);
            entity.IsShortcut = string.Equals(Path.GetExtension(newNormalized), ".lnk", StringComparison.OrdinalIgnoreCase);
            entity.Extension = entity.IsDirectory ? null : Path.GetExtension(newNormalized);
            entity.LastSeenUtc = DateTime.UtcNow;
            SaveLocked();
        }
    }

    private void Load()
    {
        try
        {
            if (!File.Exists(_registryFilePath))
                return;

            string json = File.ReadAllText(_registryFilePath);
            var entities = JsonSerializer.Deserialize<List<DesktopEntityModel>>(json);
            if (entities == null)
                return;

            lock (_lock)
            {
                _byId.Clear();
                _byPath.Clear();
                foreach (DesktopEntityModel entity in entities)
                {
                    _byId[entity.Id] = entity;
                    if (!string.IsNullOrWhiteSpace(entity.FileSystemPath))
                        _byPath[NormalizePath(entity.FileSystemPath)] = entity.Id;
                }
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "DesktopEntityRegistryService: failed to load registry");
        }
    }

    private void SaveLocked()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_registryFilePath)!);
            var list = _byId.Values.OrderBy(e => e.DisplayName).ToList();
            string json = JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(_registryFilePath, json);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "DesktopEntityRegistryService: failed to save registry");
        }
    }

    private static string NormalizePath(string path)
    {
        try
        {
            return Path.GetFullPath(path).TrimEnd('\\');
        }
        catch
        {
            return path.TrimEnd('\\');
        }
    }
}
