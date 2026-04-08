using IVOESpaces.Core.Models;
using Serilog;

namespace IVOESpaces.Core.Services;

public sealed class DragDropPolicyService
{
    private static readonly Lazy<DragDropPolicyService> _instance = new(() => new DragDropPolicyService());

    public static DragDropPolicyService Instance => _instance.Value;

    /// <summary>
    /// Optional callback for confirming external drops.
    /// Set by the Shell layer to show a Win32 dialog.
    /// Parameters: (fileName, spaceTitle) → true to proceed, false to cancel.
    /// </summary>
    public Func<string, string, bool>? ConfirmDropCallback { get; set; }

    private DragDropPolicyService()
    {
    }

    public SpaceItemModel? ImportIntoSpace(SpaceModel space, string fullPath)
    {
        AppSettings settings = AppSettingsRepository.Instance.Current;

        if (settings.ConfirmExternalDrops)
        {
            string fileName = Path.GetFileName(fullPath);
            if (ConfirmDropCallback == null)
            {
                Log.Warning("DragDropPolicy: external drop blocked for '{Path}' into '{Space}' because no confirmation callback is registered", fullPath, space.Title);
                return null;
            }

            if (!ConfirmDropCallback(fileName, space.Title))
            {
                Log.Information("DragDropPolicy: user declined drop of '{Path}' into '{Space}'", fullPath, space.Title);
                return null;
            }
        }

        if (space.Type == SpaceType.Portal)
            return ImportReference(space, fullPath);

        return settings.StandardSpaceDropMode switch
        {
            "MoveIntoSpaceStorage" => ImportOwnedCopy(space, fullPath),
            "Shortcut" => ImportShortcut(space, fullPath),
            _ => ImportReference(space, fullPath),
        };
    }

    private static SpaceItemModel? ImportReference(SpaceModel space, string fullPath)
    {
        if (!SpaceImportService.ImportPathIntoSpace(space, fullPath))
            return null;

        SpaceItemModel item = space.Items.Last();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static SpaceItemModel? ImportOwnedCopy(SpaceModel space, string fullPath)
    {
        bool isDirectory = Directory.Exists(fullPath);
        bool isFile = File.Exists(fullPath);
        if (!isDirectory && !isFile)
            return null;

        var item = new SpaceItemModel
        {
            Id = Guid.NewGuid(),
            TargetPath = fullPath,
            DisplayName = isDirectory ? Path.GetFileName(fullPath) : Path.GetFileNameWithoutExtension(fullPath),
            IsDirectory = isDirectory,
            IsFromDesktop = true,
            SortOrder = space.Items.Count,
        };

        if (!SpaceFileOwnershipService.Instance.EnsureSpaceItemOwnership(space, item))
            return null;

        if (space.Items.Any(existing => string.Equals(existing.TargetPath, item.TargetPath, StringComparison.OrdinalIgnoreCase)))
            return null;

        space.Items.Add(item);
        SpaceStateService.Instance.MarkDirty();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static SpaceItemModel? ImportShortcut(SpaceModel space, string fullPath)
    {
        string shortcutPath = CreateShortcutForPath(fullPath);
        if (string.IsNullOrWhiteSpace(shortcutPath))
            return null;

        if (!SpaceImportService.ImportPathIntoSpace(space, shortcutPath))
            return null;

        SpaceItemModel item = space.Items.Last();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static void ApplyRulesIfEnabled(SpaceItemModel item)
    {
        if (!AppSettingsRepository.Instance.Current.AutoApplyRulesOnDrop)
            return;

        Guid? targetSpaceId = SortRulesEngine.Instance.DetermineSpaceForItem(item);
        if (!targetSpaceId.HasValue)
            return;

        SpaceModel? targetSpace = SpaceStateService.Instance.GetSpace(targetSpaceId.Value);
        if (targetSpace == null)
            return;

        // Find the source space that currently contains this item
        SpaceModel? sourceSpace = null;
        foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
        {
            if (space.Items.Any(i => i.Id == item.Id))
            {
                sourceSpace = space;
                break;
            }
        }

        if (sourceSpace == null || sourceSpace.Id == targetSpace.Id)
            return;

        // Move item from source to target space
        sourceSpace.Items.RemoveAll(i => i.Id == item.Id);
        item.SortOrder = targetSpace.Items.Count;
        targetSpace.Items.Add(item);
        SpaceStateService.Instance.MarkDirty();

        Log.Information(
            "DragDropPolicy: auto-applied rule — moved '{Name}' from '{Source}' to '{Target}'",
            item.DisplayName, sourceSpace.Title, targetSpace.Title);
    }

    private static string CreateShortcutForPath(string fullPath)
    {
        try
        {
            Directory.CreateDirectory(AppPaths.StorageRoot);
            string baseName = Path.GetFileNameWithoutExtension(fullPath);
            string shortcutPath = Path.Combine(AppPaths.StorageRoot, $"{baseName}.lnk");
            int suffix = 1;

            while (File.Exists(shortcutPath))
            {
                shortcutPath = Path.Combine(AppPaths.StorageRoot, $"{baseName} ({suffix}).lnk");
                suffix++;
            }

            Type? shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                return string.Empty;

            dynamic shell = Activator.CreateInstance(shellType)!;
            dynamic shortcut = shell.CreateShortcut(shortcutPath);
            shortcut.TargetPath = fullPath;
            shortcut.WorkingDirectory = Path.GetDirectoryName(fullPath) ?? AppPaths.StorageRoot;
            shortcut.Save();
            return shortcutPath;
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "DragDropPolicy: failed to create shortcut for '{Path}'", fullPath);
            return string.Empty;
        }
    }
}