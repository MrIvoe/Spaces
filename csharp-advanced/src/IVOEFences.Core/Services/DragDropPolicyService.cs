using IVOEFences.Core.Models;
using Serilog;

namespace IVOEFences.Core.Services;

public sealed class DragDropPolicyService
{
    private static readonly Lazy<DragDropPolicyService> _instance = new(() => new DragDropPolicyService());

    public static DragDropPolicyService Instance => _instance.Value;

    /// <summary>
    /// Optional callback for confirming external drops.
    /// Set by the Shell layer to show a Win32 dialog.
    /// Parameters: (fileName, fenceTitle) → true to proceed, false to cancel.
    /// </summary>
    public Func<string, string, bool>? ConfirmDropCallback { get; set; }

    private DragDropPolicyService()
    {
    }

    public FenceItemModel? ImportIntoFence(FenceModel fence, string fullPath)
    {
        AppSettings settings = AppSettingsRepository.Instance.Current;

        if (settings.ConfirmExternalDrops)
        {
            string fileName = Path.GetFileName(fullPath);
            if (ConfirmDropCallback == null)
            {
                Log.Warning("DragDropPolicy: external drop blocked for '{Path}' into '{Fence}' because no confirmation callback is registered", fullPath, fence.Title);
                return null;
            }

            if (!ConfirmDropCallback(fileName, fence.Title))
            {
                Log.Information("DragDropPolicy: user declined drop of '{Path}' into '{Fence}'", fullPath, fence.Title);
                return null;
            }
        }

        if (fence.Type == FenceType.Portal)
            return ImportReference(fence, fullPath);

        return settings.StandardFenceDropMode switch
        {
            "MoveIntoFenceStorage" => ImportOwnedCopy(fence, fullPath),
            "Shortcut" => ImportShortcut(fence, fullPath),
            _ => ImportReference(fence, fullPath),
        };
    }

    private static FenceItemModel? ImportReference(FenceModel fence, string fullPath)
    {
        if (!FenceImportService.ImportPathIntoFence(fence, fullPath))
            return null;

        FenceItemModel item = fence.Items.Last();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static FenceItemModel? ImportOwnedCopy(FenceModel fence, string fullPath)
    {
        bool isDirectory = Directory.Exists(fullPath);
        bool isFile = File.Exists(fullPath);
        if (!isDirectory && !isFile)
            return null;

        var item = new FenceItemModel
        {
            Id = Guid.NewGuid(),
            TargetPath = fullPath,
            DisplayName = isDirectory ? Path.GetFileName(fullPath) : Path.GetFileNameWithoutExtension(fullPath),
            IsDirectory = isDirectory,
            IsFromDesktop = true,
            SortOrder = fence.Items.Count,
        };

        if (!FenceFileOwnershipService.Instance.EnsureFenceItemOwnership(fence, item))
            return null;

        if (fence.Items.Any(existing => string.Equals(existing.TargetPath, item.TargetPath, StringComparison.OrdinalIgnoreCase)))
            return null;

        fence.Items.Add(item);
        FenceStateService.Instance.MarkDirty();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static FenceItemModel? ImportShortcut(FenceModel fence, string fullPath)
    {
        string shortcutPath = CreateShortcutForPath(fullPath);
        if (string.IsNullOrWhiteSpace(shortcutPath))
            return null;

        if (!FenceImportService.ImportPathIntoFence(fence, shortcutPath))
            return null;

        FenceItemModel item = fence.Items.Last();
        ApplyRulesIfEnabled(item);
        return item;
    }

    private static void ApplyRulesIfEnabled(FenceItemModel item)
    {
        if (!AppSettingsRepository.Instance.Current.AutoApplyRulesOnDrop)
            return;

        Guid? targetFenceId = SortRulesEngine.Instance.DetermineFenceForItem(item);
        if (!targetFenceId.HasValue)
            return;

        FenceModel? targetFence = FenceStateService.Instance.GetFence(targetFenceId.Value);
        if (targetFence == null)
            return;

        // Find the source fence that currently contains this item
        FenceModel? sourceFence = null;
        foreach (FenceModel fence in FenceStateService.Instance.Fences)
        {
            if (fence.Items.Any(i => i.Id == item.Id))
            {
                sourceFence = fence;
                break;
            }
        }

        if (sourceFence == null || sourceFence.Id == targetFence.Id)
            return;

        // Move item from source to target fence
        sourceFence.Items.RemoveAll(i => i.Id == item.Id);
        item.SortOrder = targetFence.Items.Count;
        targetFence.Items.Add(item);
        FenceStateService.Instance.MarkDirty();

        Log.Information(
            "DragDropPolicy: auto-applied rule — moved '{Name}' from '{Source}' to '{Target}'",
            item.DisplayName, sourceFence.Title, targetFence.Title);
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