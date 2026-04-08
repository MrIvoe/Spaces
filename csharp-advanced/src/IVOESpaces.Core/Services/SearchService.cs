using System;
using System.Collections.Generic;
using System.Linq;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 38: Search result item returned from search queries.
/// </summary>
public sealed record SearchResultItem
{
    public Guid ItemId { get; init; }
    public Guid SpaceId { get; init; }
    public string DisplayName { get; init; } = string.Empty;
    public string TargetPath { get; init; } = string.Empty;
    public string SpaceTitle { get; init; } = string.Empty;
    public double RelevanceScore { get; init; } // 0.0 - 1.0
}

/// <summary>
/// Step 38: Search scope enumeration.
/// </summary>
public enum SearchScope
{
    CurrentSpace,   // Search only in focused space
    AllSpaces,      // Search across all spaces
    CurrentPage,    // Search spaces on current desktop page
}

/// <summary>
/// Step 38: Service for searching across spaces and items.
/// Executes queries and returns ranked results.
/// </summary>
public sealed class SearchService
{
    private static SearchService? _instance;
    private static readonly object _lock = new();

    public static SearchService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new SearchService();
                }
            }
            return _instance;
        }
    }

    private SearchService()
    {
    }

    /// <summary>
    /// Step 38: Execute a search query across spaces and items.
    /// Returns ranked results sorted by relevance.
    /// </summary>
    public List<SearchResultItem> Search(string query, SearchScope scope = SearchScope.AllSpaces, Guid? currentSpaceId = null, int currentPageIndex = 0)
    {
        if (string.IsNullOrWhiteSpace(query))
            return new List<SearchResultItem>();

        try
        {
            var results = new List<SearchResultItem>();
            var normalizedQuery = query.ToLowerInvariant();

            var allSpaces = SpaceStateService.Instance.Spaces;

            // Filter spaces based on scope
            var targetSpaces = scope switch
            {
                SearchScope.CurrentSpace => currentSpaceId.HasValue 
                    ? allSpaces.Where(f => f.Id == currentSpaceId.Value).ToList()
                    : new List<SpaceModel>(),

                SearchScope.CurrentPage => allSpaces
                    .Where(f => f.PageIndex == currentPageIndex && !f.TabContainerId.HasValue)
                    .ToList(),

                SearchScope.AllSpaces => allSpaces
                    .Where(f => !f.TabContainerId.HasValue) // Exclude tabbed spaces
                    .ToList(),

                _ => allSpaces.ToList()
            };

            // Search items in each space
            foreach (var space in targetSpaces)
            {
                if (space.Items == null || space.Items.Count == 0)
                    continue;

                foreach (var item in space.Items)
                {
                    var score = CalculateRelevance(normalizedQuery, item);
                    
                    if (score > 0)
                    {
                        string effectiveDisplayName = SpaceItemResolver.Instance.GetDisplayName(item);
                        string effectivePath = SpaceItemResolver.Instance.GetPath(item);

                        results.Add(new SearchResultItem
                        {
                            ItemId = item.Id,
                            SpaceId = space.Id,
                            DisplayName = effectiveDisplayName,
                            TargetPath = effectivePath,
                            SpaceTitle = space.Title,
                            RelevanceScore = score
                        });
                    }
                }
            }

            // Sort by relevance (descending)
            return results
                .OrderByDescending(r => r.RelevanceScore)
                .ThenBy(r => r.DisplayName)
                .ToList();
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Search failed for query: {Query}", query);
            return new List<SearchResultItem>();
        }
    }

    /// <summary>
    /// Step 38: Calculate relevance score for a single item (0.0 - 1.0).
    /// Higher score = better match.
    /// </summary>
    private double CalculateRelevance(string query, SpaceItemModel item)
    {
        double score = 0.0;

        string effectiveDisplayName = SpaceItemResolver.Instance.GetDisplayName(item);
        string effectivePath = SpaceItemResolver.Instance.GetPath(item);

        var lowerName = effectiveDisplayName.ToLowerInvariant();
        var lowerPath = (effectivePath ?? string.Empty).ToLowerInvariant();

        // Exact match in name (highest priority)
        if (lowerName == query)
            return 1.0;

        // Name starts with query (very high priority)
        if (lowerName.StartsWith(query))
            score += 0.9;
        // Name contains query (high priority)
        else if (lowerName.Contains(query))
            score += 0.7;

        // Path contains query (medium priority)
        if (lowerPath.Contains(query))
            score += 0.3;

        if (item.IsPinned)
            score += 0.1;

        if (item.OpenCount > 20)
            score += 0.1;
        else if (item.OpenCount > 5)
            score += 0.05;

        // Clamp to 0-1 range
        score = Math.Max(0, Math.Min(1.0, score));

        return score;
    }

    /// <summary>
    /// Step 38: Perform a case-insensitive substring search.
    /// Simpler variant for basic queries.
    /// </summary>
    public List<SearchResultItem> QuickSearch(string query)
    {
        return Search(query, SearchScope.AllSpaces);
    }

    /// <summary>
    /// Step 38: Get all items from all spaces (for unfiltered browsing).
    /// </summary>
    public List<SearchResultItem> GetAllItems()
    {
        var results = new List<SearchResultItem>();

        try
        {
            var allSpaces = SpaceStateService.Instance.Spaces;

            foreach (var space in allSpaces.Where(f => !f.TabContainerId.HasValue))
            {
                if (space.Items == null) continue;

                foreach (var item in space.Items)
                {
                    results.Add(new SearchResultItem
                    {
                        ItemId = item.Id,
                        SpaceId = space.Id,
                        DisplayName = item.DisplayName,
                        TargetPath = item.TargetPath,
                        SpaceTitle = space.Title,
                        RelevanceScore = 1.0
                    });
                }
            }
        }
        catch { }

        return results;
    }

    /// <summary>
    /// Step 38: Delete an item from a space by its IDs.
    /// </summary>
    public bool DeleteItem(Guid spaceId, Guid itemId)
    {
        try
        {
            var stateService = SpaceStateService.Instance;
            var space = stateService.GetSpace(spaceId);

            if (space == null || space.Items == null)
                return false;

            var item = space.Items.FirstOrDefault(i => i.Id == itemId);
            if (item == null)
                return false;

            space.Items.Remove(item);
            stateService.MarkDirty();

            Serilog.Log.Information("Item deleted from space: {ItemId} ← {SpaceId}",
                itemId, spaceId);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to delete item {ItemId}", itemId);
            return false;
        }
    }

    /// <summary>
    /// Step 38: Get items from a specific space.
    /// </summary>
    public List<SearchResultItem> GetSpaceItems(Guid spaceId)
    {
        var results = new List<SearchResultItem>();

        try
        {
            var space = SpaceStateService.Instance.GetSpace(spaceId);

            if (space == null || space.Items == null)
                return results;

            foreach (var item in space.Items)
            {
                results.Add(new SearchResultItem
                {
                    ItemId = item.Id,
                    SpaceId = space.Id,
                    DisplayName = item.DisplayName,
                    TargetPath = item.TargetPath,
                    SpaceTitle = space.Title,
                    RelevanceScore = 1.0
                });
            }
        }
        catch { }

        return results;
    }
}
