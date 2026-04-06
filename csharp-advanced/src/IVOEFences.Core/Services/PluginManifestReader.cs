using System.Text.Json;

namespace IVOEFences.Core.Services;

public sealed class PluginManifestReader
{
    private static readonly JsonSerializerOptions _jsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public sealed record PluginManifest(
        string Id,
        string Name,
        string Version,
        string Description,
        string Author,
        IReadOnlyList<string> Capabilities,
        string? Entry,
        string? Assembly,
        string DirectoryPath,
        string ManifestPath,
        string? MinHostVersion = null,
        string? MaxHostVersion = null,
        string? UpdateFeedUrl = null,
        string? LatestVersion = null,
        string? PackageUrl = null,
        string? PackageChecksumSha256 = null,
        string? CompatibilityNotes = null,
        string? SigningKeyId = null,
        string? ManifestSignatureBase64 = null,
        string? PackageSignatureBase64 = null);

    private sealed class PluginManifestDto
    {
        public string? Id { get; set; }
        public string? Name { get; set; }
        public string? Version { get; set; }
        public string? Description { get; set; }
        public string? Author { get; set; }
        public string[]? Capabilities { get; set; }
        public string? Entry { get; set; }
        public string? Assembly { get; set; }
        public string? MinHostVersion { get; set; }
        public string? MaxHostVersion { get; set; }
        public string? UpdateFeedUrl { get; set; }
        public string? LatestVersion { get; set; }
        public string? PackageUrl { get; set; }
        public string? PackageChecksumSha256 { get; set; }
        public string? CompatibilityNotes { get; set; }
        public string? SigningKeyId { get; set; }
        public string? ManifestSignatureBase64 { get; set; }
        public string? PackageSignatureBase64 { get; set; }
    }

    public IReadOnlyList<PluginManifest> ReadAll(string pluginsRoot)
    {
        if (string.IsNullOrWhiteSpace(pluginsRoot) || !Directory.Exists(pluginsRoot))
            return Array.Empty<PluginManifest>();

        List<PluginManifest> manifests = new();
        foreach (string dir in Directory.EnumerateDirectories(pluginsRoot))
        {
            string manifestPath = Path.Combine(dir, "plugin.json");
            if (!File.Exists(manifestPath))
                continue;

            if (TryReadManifest(manifestPath, out PluginManifest? manifest) && manifest != null)
                manifests.Add(manifest);
        }

        return manifests;
    }

    public IReadOnlyList<PluginManifest> ReadInstalled(string installedRoot)
    {
        if (string.IsNullOrWhiteSpace(installedRoot) || !Directory.Exists(installedRoot))
            return Array.Empty<PluginManifest>();

        List<PluginManifest> manifests = new();
        foreach (string pluginIdDir in Directory.EnumerateDirectories(installedRoot))
        {
            foreach (string versionDir in Directory.EnumerateDirectories(pluginIdDir))
            {
                string manifestPath = Path.Combine(versionDir, "plugin.json");
                if (!File.Exists(manifestPath))
                    continue;

                if (TryReadManifest(manifestPath, out PluginManifest? manifest) && manifest != null)
                    manifests.Add(manifest);
            }
        }

        return manifests;
    }

    public bool TryReadManifest(string manifestPath, out PluginManifest? manifest)
    {
        manifest = null;
        try
        {
            if (!File.Exists(manifestPath))
                return false;

            string json = File.ReadAllText(manifestPath);
            PluginManifestDto? dto = JsonSerializer.Deserialize<PluginManifestDto>(json, _jsonOptions);
            if (dto == null || string.IsNullOrWhiteSpace(dto.Id) || string.IsNullOrWhiteSpace(dto.Name))
                return false;

            string directoryPath = Path.GetDirectoryName(manifestPath) ?? string.Empty;
            manifest = new PluginManifest(
                dto.Id,
                dto.Name,
                string.IsNullOrWhiteSpace(dto.Version) ? "0.0.0" : dto.Version,
                dto.Description ?? string.Empty,
                dto.Author ?? "unknown",
                dto.Capabilities ?? Array.Empty<string>(),
                dto.Entry,
                dto.Assembly,
                directoryPath,
                manifestPath,
                dto.MinHostVersion,
                dto.MaxHostVersion,
                dto.UpdateFeedUrl,
                dto.LatestVersion,
                dto.PackageUrl,
                dto.PackageChecksumSha256,
                dto.CompatibilityNotes,
                dto.SigningKeyId,
                dto.ManifestSignatureBase64,
                dto.PackageSignatureBase64);

            return true;
        }
        catch
        {
            return false;
        }
    }
}
