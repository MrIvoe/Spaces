using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// AI-driven icon grouping suggester.
/// Analyzes usage patterns, file types, and existing space structures
/// to recommend which space(s) an icon should belong to.
/// 
/// Uses rule-based scoring:
/// - File type matching (executables, documents, media, etc.)
/// - Usage recency and frequency
/// - Space category inference from existing icons
/// - Similarity to existing icons in space
/// </summary>
public sealed class SpaceIconGroupingSuggester
{
    private static readonly Lazy<SpaceIconGroupingSuggester> _instance =
        new(() => new SpaceIconGroupingSuggester());

    public static SpaceIconGroupingSuggester Instance => _instance.Value;

    private SpaceIconGroupingSuggester()
    {
    }

    /// <summary>Suggestion result with confidence score and reason.</summary>
    public record GroupingSuggestion(SpaceModel Space, double ConfidenceScore, string Reason);

    /// <summary>
    /// Get top N space suggestions for moving an icon.
    /// Returns suggestions ranked by confidence (highest first).
    /// </summary>
    public List<GroupingSuggestion> GetSpaceSuggestions(
        SpaceItemModel item, IEnumerable<SpaceModel> spaces, int topN = 3)
    {
        var suggestions = new List<GroupingSuggestion>();
        var tracker = SpaceUsageTracker.Instance;

        foreach (var space in spaces)
        {
            // Skip if item already in this space
            if (space.Items.Any(i => i.Id == item.Id))
                continue;

            double score = CalculateFitScore(item, space, tracker);
            if (score > 0)
            {
                string reason = GetSuggestionReason(item, space);
                suggestions.Add(new GroupingSuggestion(space, score, reason));
            }
        }

        return suggestions
            .OrderByDescending(s => s.ConfidenceScore)
            .Take(topN)
            .ToList();
    }

    /// <summary>
    /// Determine the inferred "category" of a space based on its icon types.
    /// Returns: "Apps", "Documents", "Media", "Mixed", or null if empty.
    /// </summary>
    public static string? InferSpaceCategory(SpaceModel space)
    {
        if (space.Items.Count == 0)
            return null;

        var typeGroups = space.Items
            .Where(i => !string.IsNullOrEmpty(i.TrackedFileType))
            .GroupBy(i => ClassifyFileType(i.TrackedFileType!))
            .OrderByDescending(g => g.Count())
            .ToList();

        if (typeGroups.Count == 0)
            return null;

        var topCategory = typeGroups[0].Key;
        int topCount = typeGroups[0].Count();
        int total = space.Items.Count;

        // If >60% of icons fall into one category, that's the space category
        if (topCount >= (total * 0.6))
            return topCategory;

        return "Mixed";
    }

    /// <summary>Classify a file type into a broad category.</summary>
    private static string ClassifyFileType(string extension)
    {
        if (string.IsNullOrEmpty(extension))
            return "Other";

        extension = extension.ToLowerInvariant();

        // Executables
        if (extension is ".exe" or ".com" or ".bat" or ".cmd")
            return "Apps";

        // Shortcuts
        if (extension is ".lnk" or ".url")
            return "Shortcuts";

        // Documents
        if (extension is ".docx" or ".doc" or ".pdf" or ".txt" or ".xlsx" or
                ".xls" or ".pptx" or ".ppt" or ".odt" or ".rtf")
            return "Documents";

        // Media (video)
        if (extension is ".mp4" or ".mkv" or ".avi" or ".mov" or ".flv" or
                ".wmv" or ".webm")
            return "Video";

        // Media (audio)
        if (extension is ".mp3" or ".wav" or ".flac" or ".aac" or ".ogg" or
                ".m4a" or ".wma")
            return "Audio";

        // Images
        if (extension is ".jpg" or ".jpeg" or ".png" or ".gif" or ".bmp" or
                ".svg" or ".webp" or ".tiff")
            return "Images";

        // Compressed
        if (extension is ".zip" or ".7z" or ".rar" or ".iso" or ".tar" or
                ".gz" or ".xz")
            return "Archives";

        // Code/development
        if (extension is ".cs" or ".cpp" or ".c" or ".js" or ".py" or ".java" or
                ".sln" or ".vcxproj" or ".csproj")
            return "Code";

        // Directory
        if (extension == "<DIR>")
            return "Folders";

        return "Other";
    }

    /// <summary>Calculate fit score (0.0–1.0) for an item in a space.</summary>
    private static double CalculateFitScore(
        SpaceItemModel item, SpaceModel space, SpaceUsageTracker tracker)
    {
        double score = 0.0;

        // 1. Category matching (40% weight)
        string? itemCategory = ClassifyFileType(item.TrackedFileType ?? string.Empty);
        string? spaceCategory = InferSpaceCategory(space);
        if (!string.IsNullOrEmpty(itemCategory) && !string.IsNullOrEmpty(spaceCategory))
        {
            if (itemCategory == spaceCategory && spaceCategory != "Mixed")
                score += 0.40; // strong match
            else if (spaceCategory == "Mixed")
                score += 0.20; // misc space accepts anything
        }

        // 2. Usage prominence (30% weight)
        // Put frequently/recently used items in visible spaces
        double recency = tracker.GetRecencyScore(item);
        double frequency = tracker.GetFrequencyScore(item);
        double usageScore = (recency * 0.4) + (frequency * 0.6);
        score += usageScore * 0.30;

        // 3. Space name heuristic (15% weight)
        // If space title contains keywords matching the item type, boost score
        string spaceTitle = space.Title.ToLowerInvariant();
        if ((itemCategory == "Documents" && spaceTitle.Contains("doc")) ||
            (itemCategory == "Apps" && (spaceTitle.Contains("app") || spaceTitle.Contains("util"))) ||
            (itemCategory == "Media" && (spaceTitle.Contains("media") || spaceTitle.Contains("video"))) ||
            (itemCategory == "Folders" && spaceTitle.Contains("folder")))
        {
            score += 0.15;
        }

        // 4. Similarity to existing icons in space (15% weight)
        // If space already contains similar-typed items, boost
        int similarCount = space.Items.Count(i =>
            ClassifyFileType(i.TrackedFileType ?? string.Empty) == itemCategory);
        if (similarCount > 0)
        {
            double similarityRatio = (double)similarCount / Math.Max(1, space.Items.Count);
            score += similarityRatio * 0.15;
        }

        // Clamp to 0.0–1.0
        return Math.Min(1.0, Math.Max(0.0, score));
    }

    /// <summary>Generate human-readable reason for suggestion.</summary>
    private static string GetSuggestionReason(SpaceItemModel item, SpaceModel space)
    {
        string itemType = ClassifyFileType(item.TrackedFileType ?? string.Empty);
        string? spaceCategory = InferSpaceCategory(space);

        if (spaceCategory == itemType && spaceCategory != "Mixed")
            return $"Matches {itemType} category";

        if (spaceCategory == "Mixed")
            return "Compatible with miscellaneous items";

        if (space.Title.ToLowerInvariant().Contains(itemType.ToLowerInvariant()))
            return $"Space title suggests {itemType}";

        var similarCount = space.Items.Count(i =>
            ClassifyFileType(i.TrackedFileType ?? string.Empty) == itemType);
        if (similarCount > 0)
            return $"{similarCount} {itemType} items already here";

        return "Good fit for this space";
    }

    /// <summary>
    /// Get a "smart sort order" for icons within a space based on usage.
    /// Returns item IDs sorted by combined score (recency + frequency).
    /// </summary>
    public List<Guid> GetSmartSortOrder(SpaceModel space, bool recentFirst = true)
    {
        var tracker = SpaceUsageTracker.Instance;
        var sorted = space.Items
            .Select(item => new
            {
                Item = item,
                Score = (tracker.GetRecencyScore(item) * 0.6) +
                       (tracker.GetFrequencyScore(item) * 0.4)
            })
            .OrderByDescending(x => x.Score)
            .Select(x => x.Item.Id)
            .ToList();

        if (!recentFirst)
            sorted.Reverse();

        return sorted;
    }
}
