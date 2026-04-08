using System.Text;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Live search/filter for space items using fuzzy matching.
/// 
/// Features:
/// - Real-time filtering as user types
/// - Fuzzy matching (e.g., "doc" matches "Desktop_Document.docx")
/// - Case-insensitive search
/// - Maintains sort order of matched items
/// </summary>
public sealed class SpaceSearchFilter
{
    private string _query = string.Empty;
    private List<SpaceItemModel>? _filteredCache;
    private bool _cacheValid;

    /// <summary>Current search query.</summary>
    public string Query
    {
        get => _query;
        set
        {
            if (_query != value)
            {
                _query = value;
                _cacheValid = false;
            }
        }
    }

    /// <summary>Is filter currently active (query not empty)?</summary>
    public bool IsActive => !string.IsNullOrWhiteSpace(_query);

    /// <summary>Clear the current filter.</summary>
    public void Clear()
    {
        Query = string.Empty;
    }

    /// <summary>Get filtered items (cached until Query changes).</summary>
    public IReadOnlyList<SpaceItemModel> GetFilteredItems(IReadOnlyList<SpaceItemModel> allItems)
    {
        if (!IsActive)
            return allItems;

        if (_cacheValid && _filteredCache != null)
            return _filteredCache;

        _filteredCache = allItems
            .Where(item => FuzzyMatch(item.DisplayName, _query))
            .ToList();

        _cacheValid = true;
        return _filteredCache;
    }

    /// <summary>
    /// Fuzzy match: checks if query characters appear in sequence in the target string.
    /// Example: "doc" matches "Desktop_Document.docx" (d-o-c in sequence).
    /// </summary>
    private static bool FuzzyMatch(string target, string query)
    {
        if (string.IsNullOrEmpty(query))
            return true;

        target = target.ToLowerInvariant();
        query = query.ToLowerInvariant();

        int targetIdx = 0;
        int queryIdx = 0;

        while (targetIdx < target.Length && queryIdx < query.Length)
        {
            if (target[targetIdx] == query[queryIdx])
                queryIdx++;
            targetIdx++;
        }

        return queryIdx == query.Length;
    }

    /// <summary>
    /// Calculate relevance score for a match (0.0—1.0).
    /// Higher score = better match (e.g., exact prefix match scores higher than scattered match).
    /// </summary>
    public static double GetMatchScore(string target, string query)
    {
        if (string.IsNullOrEmpty(query))
            return 1.0;

        target = target.ToLowerInvariant();
        query = query.ToLowerInvariant();

        // Perfect match
        if (target == query)
            return 1.0;

        // Starts with query
        if (target.StartsWith(query))
            return 0.9;

        // Contains query as substring
        if (target.Contains(query))
            return 0.7;

        // Fuzzy match (scattered characters)
        int score = 0;
        int targetIdx = 0;

        foreach (char qChar in query)
        {
            int idx = target.IndexOf(qChar, targetIdx);
            if (idx < 0) return 0.0;
            score += (targetIdx == 0 && idx == 0) ? 10 : 1; // bonus for first char match
            targetIdx = idx + 1;
        }

        return Math.Min(0.5 + (score * 0.05), 0.69);
    }
}
