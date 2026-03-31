using System.Text.Json;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Fast workspace switching via desktop links.
/// Real files remain in storage; each workspace controls which links are projected.
/// </summary>
public sealed class WorkspaceLinkService
{
    private static readonly Lazy<WorkspaceLinkService> _instance = new(() => new WorkspaceLinkService());
    public static WorkspaceLinkService Instance => _instance.Value;

    private WorkspaceLinkService()
    {
    }

    public sealed class WorkspaceSwitchResult
    {
        public int Removed { get; init; }
        public int Created { get; init; }
        public int Failed { get; init; }
    }

    private sealed record WorkspaceEntry(string ShortcutPath, string TargetPath);

    public WorkspaceSwitchResult SwitchWorkspace(FenceProfileModel profile, IReadOnlyList<FenceModel> fences)
    {
        Directory.CreateDirectory(AppPaths.WorkspaceRoot);
        Directory.CreateDirectory(AppPaths.StorageRoot);
        Directory.CreateDirectory(AppPaths.WorkspaceDesktopDir);

        var desiredItems = SelectProfileItems(profile, fences);
        var desiredLinks = BuildLinkEntries(desiredItems);

        string manifestPath = Path.Combine(AppPaths.WorkspaceRoot, $"{profile.Id}.json");
        File.WriteAllText(manifestPath, JsonSerializer.Serialize(desiredLinks, new JsonSerializerOptions { WriteIndented = true }));

        var watcher = DesktopWatcherService.Instance;
        watcher.Pause();

        int removed = 0;
        int created = 0;
        int failed = 0;

        try
        {
            foreach (string existing in Directory.EnumerateFileSystemEntries(AppPaths.WorkspaceDesktopDir))
            {
                if (TryDeleteEntry(existing))
                    removed++;
            }

            foreach (WorkspaceEntry entry in desiredLinks)
            {
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(entry.ShortcutPath)!);
                    bool ok = CreateShellShortcut(entry.ShortcutPath, entry.TargetPath);
                    if (ok) created++;
                    else failed++;
                }
                catch
                {
                    failed++;
                }
            }

            string activePath = Path.Combine(AppPaths.WorkspaceRoot, "active.json");
            File.WriteAllText(activePath, JsonSerializer.Serialize(desiredLinks, new JsonSerializerOptions { WriteIndented = true }));
        }
        finally
        {
            watcher.Resume();
        }

        Serilog.Log.Information("WorkspaceLinkService: switched profile '{Profile}' links removed={Removed} created={Created} failed={Failed}",
            profile.Name, removed, created, failed);

        return new WorkspaceSwitchResult { Removed = removed, Created = created, Failed = failed };
    }

    private static List<FenceItemModel> SelectProfileItems(FenceProfileModel profile, IReadOnlyList<FenceModel> fences)
    {
        IEnumerable<FenceModel> chosen = profile.VisibleFenceIds.Count > 0
            ? fences.Where(f => profile.VisibleFenceIds.Contains(f.Id))
            : fences;

        return chosen
            .Where(f => f.Type == FenceType.Standard)
            .SelectMany(f => f.Items)
            .Where(i => !string.IsNullOrWhiteSpace(i.TargetPath))
            .Where(i => File.Exists(i.TargetPath) || Directory.Exists(i.TargetPath))
            .ToList();
    }

    private static List<WorkspaceEntry> BuildLinkEntries(IReadOnlyList<FenceItemModel> items)
    {
        var result = new List<WorkspaceEntry>(items.Count);
        var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (FenceItemModel item in items)
        {
            string baseName = item.IsDirectory
                ? (string.IsNullOrWhiteSpace(item.DisplayName) ? Path.GetFileName(item.TargetPath) : item.DisplayName)
                : Path.GetFileName(item.TargetPath);

            if (string.IsNullOrWhiteSpace(baseName))
                baseName = item.Id.ToString("N");

            string safeName = SanitizeFileName(baseName);
            string shortcutPath = MakeUniqueShortcutPath(AppPaths.WorkspaceDesktopDir, safeName, used);
            result.Add(new WorkspaceEntry(shortcutPath, item.TargetPath));
        }

        return result;
    }

    private static bool CreateShellShortcut(string shortcutPath, string targetPath)
    {
        try
        {
            Type? shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                return false;

            dynamic shell = Activator.CreateInstance(shellType)!;
            dynamic lnk = shell.CreateShortcut(shortcutPath);
            lnk.TargetPath = targetPath;
            lnk.WorkingDirectory = Path.GetDirectoryName(targetPath) ?? Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            lnk.Save();
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "WorkspaceLinkService: shortcut create failed for '{Target}'", targetPath);
            return false;
        }
    }

    private static bool TryDeleteEntry(string path)
    {
        try
        {
            if (Directory.Exists(path))
                Directory.Delete(path, recursive: true);
            else if (File.Exists(path))
                File.Delete(path);
            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "WorkspaceLinkService: failed deleting existing workspace entry '{Path}'", path);
            return false;
        }
    }

    private static string SanitizeFileName(string name)
    {
        string s = name;
        foreach (char c in Path.GetInvalidFileNameChars())
            s = s.Replace(c, '_');
        return string.IsNullOrWhiteSpace(s) ? "item" : s;
    }

    private static string MakeUniqueShortcutPath(string root, string fileName, HashSet<string> used)
    {
        string stem = Path.GetFileNameWithoutExtension(fileName);
        string candidate = Path.Combine(root, stem + ".lnk");
        int i = 1;

        while (used.Contains(candidate) || File.Exists(candidate) || Directory.Exists(candidate))
        {
            candidate = Path.Combine(root, $"{stem} ({i}).lnk");
            i++;
        }

        used.Add(candidate);
        return candidate;
    }
}
