using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;
using IVOEFences.Core;
using IVOEFences.Core.Plugins;

namespace IVOEFences.Core.Services;

public sealed class PluginPackageService
{
    private sealed class InstallJournalEntry
    {
        public string PluginId { get; set; } = string.Empty;
        public string Version { get; set; } = string.Empty;
        public string InstallDirectory { get; set; } = string.Empty;
        public string BackupDirectory { get; set; } = string.Empty;
        public DateTimeOffset TimestampUtc { get; set; }
    }

    private static readonly Lazy<PluginPackageService> _instance = new(() => new PluginPackageService());
    private readonly object _journalLock = new();
    private readonly PluginManifestReader _manifestReader;
    private readonly PluginTrustPolicyService _trustPolicy;

    public static PluginPackageService Instance => _instance.Value;

    private PluginPackageService()
    {
        _manifestReader = new PluginManifestReader();
        _trustPolicy = PluginTrustPolicyService.Instance;
    }

    public PluginInstallResult InstallPackageFromArchive(string packageFilePath, string? expectedChecksumHex = null)
    {
        if (string.IsNullOrWhiteSpace(packageFilePath) || !File.Exists(packageFilePath))
            return new PluginInstallResult(false, "Package file was not found.");

        string transactionId = Guid.NewGuid().ToString("N");
        string stageDir = Path.Combine(AppPaths.PluginStagingDir, "txn", transactionId);
        string? installDir = null;
        string? backupDir = null;
        string? pluginId = null;
        string? version = null;

        try
        {
            Directory.CreateDirectory(stageDir);
            ZipFile.ExtractToDirectory(packageFilePath, stageDir, overwriteFiles: true);

            string? manifestPath = Directory
                .EnumerateFiles(stageDir, "plugin.json", SearchOption.AllDirectories)
                .FirstOrDefault();

            if (string.IsNullOrWhiteSpace(manifestPath) ||
                !_manifestReader.TryReadManifest(manifestPath, out PluginManifestReader.PluginManifest? manifest) ||
                manifest == null)
            {
                return new PluginInstallResult(false, "Package does not contain a valid plugin.json manifest.");
            }

            pluginId = manifest.Id;
            version = manifest.Version;

            _trustPolicy.Reload();
            PluginTrustDecision manifestTrust = _trustPolicy.EvaluateManifest(manifest);
            if (!manifestTrust.IsTrusted)
                return new PluginInstallResult(false, $"Manifest trust verification failed: {manifestTrust.Reason}", pluginId, version);

            string packageChecksum = ComputeSha256Hex(packageFilePath);
            string expected = string.IsNullOrWhiteSpace(expectedChecksumHex)
                ? manifest.PackageChecksumSha256 ?? string.Empty
                : expectedChecksumHex.Trim();

            if (!string.IsNullOrWhiteSpace(expected) &&
                !string.Equals(expected, packageChecksum, StringComparison.OrdinalIgnoreCase))
            {
                return new PluginInstallResult(false,
                    $"Package checksum mismatch. Expected {expected}, got {packageChecksum}.",
                    pluginId,
                    version);
            }

            PluginTrustDecision packageTrust = _trustPolicy.EvaluatePackageSignature(manifest, packageChecksum);
            if (!packageTrust.IsTrusted)
                return new PluginInstallResult(false, $"Package signature verification failed: {packageTrust.Reason}", pluginId, version);

            installDir = Path.Combine(AppPaths.PluginInstalledDir, manifest.Id, manifest.Version);
            backupDir = Path.Combine(AppPaths.PluginInstalledDir, manifest.Id, $"backup-{manifest.Version}-{DateTime.UtcNow:yyyyMMddHHmmss}");

            if (Directory.Exists(installDir))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(backupDir) ?? AppPaths.PluginInstalledDir);
                Directory.Move(installDir, backupDir);
            }

            Directory.CreateDirectory(Path.GetDirectoryName(installDir) ?? AppPaths.PluginInstalledDir);
            CopyDirectory(manifest.DirectoryPath, installDir);
            WriteJournalEntry(manifest.Id, manifest.Version, installDir, backupDir);

            return new PluginInstallResult(true,
                "Package installed successfully.",
                manifest.Id,
                manifest.Version,
                installDir,
                backupDir);
        }
        catch (Exception ex)
        {
            TryRollbackInstall_NoThrow(installDir, backupDir);
            return new PluginInstallResult(false,
                $"Package install failed and was rolled back when possible: {ex.Message}",
                pluginId,
                version,
                installDir,
                backupDir);
        }
        finally
        {
            TryDeleteDirectory_NoThrow(stageDir);
        }
    }

    public PluginInstallResult RollbackLastInstall(string pluginId)
    {
        if (string.IsNullOrWhiteSpace(pluginId))
            return new PluginInstallResult(false, "Plugin id is required for rollback.");

        InstallJournalEntry? entry = GetLastJournalEntry(pluginId);
        if (entry == null)
            return new PluginInstallResult(false, "No rollback snapshot was found for that plugin.", pluginId);

        if (string.IsNullOrWhiteSpace(entry.BackupDirectory) || !Directory.Exists(entry.BackupDirectory))
            return new PluginInstallResult(false, "Rollback snapshot no longer exists on disk.", pluginId, entry.Version);

        try
        {
            if (Directory.Exists(entry.InstallDirectory))
                Directory.Delete(entry.InstallDirectory, recursive: true);

            Directory.CreateDirectory(Path.GetDirectoryName(entry.InstallDirectory) ?? AppPaths.PluginInstalledDir);
            CopyDirectory(entry.BackupDirectory, entry.InstallDirectory);

            return new PluginInstallResult(true,
                "Rollback completed successfully.",
                pluginId,
                entry.Version,
                entry.InstallDirectory,
                entry.BackupDirectory);
        }
        catch (Exception ex)
        {
            return new PluginInstallResult(false,
                $"Rollback failed: {ex.Message}",
                pluginId,
                entry.Version,
                entry.InstallDirectory,
                entry.BackupDirectory);
        }
    }

    public string StagePackage(string packageFilePath, string pluginId, string version)
    {
        string stageDir = Path.Combine(AppPaths.PluginStagingDir, pluginId, version);
        if (Directory.Exists(stageDir))
            Directory.Delete(stageDir, recursive: true);

        Directory.CreateDirectory(stageDir);
        ZipFile.ExtractToDirectory(packageFilePath, stageDir, overwriteFiles: true);
        return stageDir;
    }

    public string InstallStagedPackage(string stagedDirectory, string pluginId, string version)
    {
        string installDir = Path.Combine(AppPaths.PluginInstalledDir, pluginId, version);
        if (Directory.Exists(installDir))
            Directory.Delete(installDir, recursive: true);

        Directory.CreateDirectory(Path.GetDirectoryName(installDir) ?? AppPaths.PluginInstalledDir);
        CopyDirectory(stagedDirectory, installDir);
        return installDir;
    }

    public IReadOnlyList<PluginManifestReader.PluginManifest> GetCandidateManifests(
        string legacyPluginsRoot,
        PluginManifestReader manifestReader)
    {
        List<PluginManifestReader.PluginManifest> manifests = new();
        manifests.AddRange(manifestReader.ReadInstalled(AppPaths.PluginInstalledDir));
        manifests.AddRange(manifestReader.ReadAll(legacyPluginsRoot));

        return manifests
            .GroupBy(m => m.Id, StringComparer.OrdinalIgnoreCase)
            .Select(g => g.OrderByDescending(m => ParseVersion(m.Version)).First())
            .ToList();
    }

    private static Version ParseVersion(string value)
    {
        return Version.TryParse(value, out Version? parsed) ? parsed : new Version(0, 0, 0);
    }

    private static string ComputeSha256Hex(string filePath)
    {
        using var stream = File.OpenRead(filePath);
        byte[] hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static void CopyDirectory(string sourceDir, string destinationDir)
    {
        Directory.CreateDirectory(destinationDir);
        foreach (string file in Directory.EnumerateFiles(sourceDir))
        {
            string target = Path.Combine(destinationDir, Path.GetFileName(file));
            File.Copy(file, target, overwrite: true);
        }

        foreach (string dir in Directory.EnumerateDirectories(sourceDir))
        {
            string targetDir = Path.Combine(destinationDir, Path.GetFileName(dir));
            CopyDirectory(dir, targetDir);
        }
    }

    private void WriteJournalEntry(string pluginId, string version, string installDirectory, string backupDirectory)
    {
        lock (_journalLock)
        {
            List<InstallJournalEntry> entries = ReadJournal_NoLock();
            entries.Add(new InstallJournalEntry
            {
                PluginId = pluginId,
                Version = version,
                InstallDirectory = installDirectory,
                BackupDirectory = backupDirectory,
                TimestampUtc = DateTimeOffset.UtcNow,
            });

            WriteJournal_NoLock(entries);
        }
    }

    private InstallJournalEntry? GetLastJournalEntry(string pluginId)
    {
        lock (_journalLock)
        {
            return ReadJournal_NoLock()
                .Where(e => string.Equals(e.PluginId, pluginId, StringComparison.OrdinalIgnoreCase))
                .OrderByDescending(e => e.TimestampUtc)
                .FirstOrDefault();
        }
    }

    private static void TryRollbackInstall_NoThrow(string? installDir, string? backupDir)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(installDir))
                return;

            if (Directory.Exists(installDir))
                Directory.Delete(installDir, recursive: true);

            if (!string.IsNullOrWhiteSpace(backupDir) && Directory.Exists(backupDir))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(installDir) ?? AppPaths.PluginInstalledDir);
                Directory.Move(backupDir, installDir);
            }
        }
        catch
        {
            // Best effort rollback.
        }
    }

    private static void TryDeleteDirectory_NoThrow(string directory)
    {
        try
        {
            if (Directory.Exists(directory))
                Directory.Delete(directory, recursive: true);
        }
        catch
        {
            // Best effort cleanup.
        }
    }

    private static List<InstallJournalEntry> ReadJournal_NoLock()
    {
        try
        {
            if (!File.Exists(AppPaths.PluginInstallJournalConfig))
                return new List<InstallJournalEntry>();

            string json = File.ReadAllText(AppPaths.PluginInstallJournalConfig);
            return JsonSerializer.Deserialize<List<InstallJournalEntry>>(json)
                ?? new List<InstallJournalEntry>();
        }
        catch
        {
            return new List<InstallJournalEntry>();
        }
    }

    private static void WriteJournal_NoLock(List<InstallJournalEntry> entries)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.PluginInstallJournalConfig) ?? AppPaths.DataRoot);
        string json = JsonSerializer.Serialize(entries, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(AppPaths.PluginInstallJournalConfig, json);
    }
}
