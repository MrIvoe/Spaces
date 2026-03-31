using System.Diagnostics;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Tracks icon usage patterns (opens, frequency, recency) for AI-based
/// icon grouping suggestions. Updates FenceItemModel metadata as items are accessed.
/// 
/// Features:
/// - Logs when an item is opened (executed/launched)
/// - Updates LastOpenedTime and OpenCount
/// - Caches file metadata (extension, size) for grouping
/// - Thread-safe across multiple fences
/// </summary>
public sealed class FenceUsageTracker
{
    private static readonly Lazy<FenceUsageTracker> _instance =
        new(() => new FenceUsageTracker());

    public static FenceUsageTracker Instance => _instance.Value;

    private readonly object _lock = new object();

    private FenceUsageTracker()
    {
    }

    /// <summary>
    /// Log that an item was opened. Updates LastOpenedTime and OpenCount.
    /// Call this when a user double-clicks/launches an icon.
    /// </summary>
    public void LogItemOpen(FenceItemModel item)
    {
        if (item == null || string.IsNullOrEmpty(FenceItemResolver.Instance.GetPath(item)))
            return;

        lock (_lock)
        {
            item.LastOpenedTime = DateTime.UtcNow;
            item.OpenCount++;

            // On first open, cache the file type
            if (string.IsNullOrEmpty(item.TrackedFileType))
                CacheFileMetadata(item);
        }
    }

    /// <summary>
    /// Log a lightweight interaction (single click/select/drag start).
    /// This increments usage frequency and refreshes last access time
    /// without implying that the item was executed.
    /// </summary>
    public void LogItemInteraction(FenceItemModel item)
    {
        if (item == null)
            return;

        lock (_lock)
        {
            item.LastOpenedTime = DateTime.UtcNow;
            item.OpenCount++;

            if (string.IsNullOrEmpty(item.TrackedFileType))
                CacheFileMetadata(item);
        }
    }

    /// <summary>
    /// Cache file metadata for grouping decisions (extension, size).
    /// Called automatically on first open or can be called explicitly to refresh.
    /// </summary>
    public void CacheFileMetadata(FenceItemModel item)
    {
        if (item == null)
            return;

        string path = FenceItemResolver.Instance.GetPath(item);
        if (string.IsNullOrEmpty(path))
            return;

        try
        {
            bool isDir = FenceItemResolver.Instance.IsDirectory(item);
            if (isDir)
            {
                item.TrackedFileType = "<DIR>";
                item.TrackedFileSize = 0;
            }
            else if (File.Exists(path))
            {
                item.TrackedFileType = Path.GetExtension(path).ToLowerInvariant();
                var info = new FileInfo(path);
                item.TrackedFileSize = info.Length;
            }
            else if (path.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase))
            {
                // Shortcut file — extract real target and get its extension
                string? realTarget = ExtractShortcutTarget(path);
                if (!string.IsNullOrEmpty(realTarget) && File.Exists(realTarget))
                {
                    item.TrackedFileType = Path.GetExtension(realTarget).ToLowerInvariant();
                    var info = new FileInfo(realTarget!);
                    item.TrackedFileSize = info.Length;
                }
                else
                {
                    item.TrackedFileType = ".lnk";
                }
            }
        }
        catch (Exception ex)
        {
            // Log but don't throw — usage tracking is best-effort
            Serilog.Log.Warning("FenceUsageTracker: Failed to cache metadata for {Target}: {Ex}",
                path, ex.Message);
        }
    }

    /// <summary>
    /// Get the recency score (0.0–1.0) of an item based on LastOpenedTime.
    /// 1.0 = opened today, 0.5 = opened a week ago, 0.0 = never opened.
    /// </summary>
    public double GetRecencyScore(FenceItemModel item)
    {
        if (item?.LastOpenedTime == null)
            return 0.0;

        var elapsed = DateTime.UtcNow - item.LastOpenedTime.Value;
        double days = elapsed.TotalDays;

        // Exponential decay: score halves every 7 days
        return Math.Pow(0.5, days / 7.0);
    }

    /// <summary>
    /// Get the frequency score (0.0–1.0) based on OpenCount.
    /// Uses logarithmic scale: 1 open = 0.1, 10 opens = 0.5, 100+ opens = 1.0.
    /// </summary>
    public double GetFrequencyScore(FenceItemModel item)
    {
        int openCount = item?.OpenCount ?? 0;
        if (openCount <= 0)
            return 0.0;

        // Logarithmic scale: log10(count) / log10(100)
        return Math.Min(1.0, Math.Log10(openCount + 1) / Math.Log10(100));
    }

    /// <summary>
    /// Normalize usage scores across a set of items (for comparison).
    /// </summary>
    public Dictionary<FenceItemModel, double> NormalizeUsageScores(
        IEnumerable<FenceItemModel> items, double recencyWeight = 0.6, double frequencyWeight = 0.4)
    {
        var scores = new Dictionary<FenceItemModel, double>();
        double maxScore = 0;

        foreach (var item in items)
        {
            double score = (GetRecencyScore(item) * recencyWeight) +
                          (GetFrequencyScore(item) * frequencyWeight);
            scores[item] = score;
            if (score > maxScore) maxScore = score;
        }

        // Normalize to 0.0–1.0
        if (maxScore > 0)
        {
            foreach (var key in scores.Keys.ToList())
                scores[key] /= maxScore;
        }

        return scores;
    }

    /// <summary>
    /// Extract the real target of a Windows shortcut (.lnk file).
    /// Returns null if unable to resolve.
    /// </summary>
    private static string? ExtractShortcutTarget(string lnkPath)
    {
        try
        {
            // Use WScript.Shell COM object to resolve shortcut
            Type? shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                return null;

            object? shellObj = Activator.CreateInstance(shellType);
            if (shellObj == null)
                return null;

            dynamic shell = shellObj;
            dynamic? link = shell.CreateShortcut(lnkPath);
            string target = link?.TargetPath as string ?? string.Empty;
            return string.IsNullOrEmpty(target) ? null : target;
        }
        catch
        {
            return null;
        }
    }
}
