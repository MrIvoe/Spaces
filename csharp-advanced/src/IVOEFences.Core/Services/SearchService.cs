using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 38: Search result item returned from search queries.
/// </summary>
public sealed record SearchResultItem
{
    public Guid ItemId { get; init; }
    public Guid FenceId { get; init; }
    public string DisplayName { get; init; } = string.Empty;
    public string TargetPath { get; init; } = string.Empty;
    public string FenceTitle { get; init; } = string.Empty;
    public double RelevanceScore { get; init; } // 0.0 - 1.0
}

/// <summary>
/// Step 38: Search scope enumeration.
/// </summary>
public enum SearchScope
{
    CurrentFence,   // Search only in focused fence
    AllFences,      // Search across all fences
    CurrentPage,    // Search fences on current desktop page
}

/// <summary>
/// Step 38: Service for searching across fences and items.
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
    /// Step 38: Execute a search query across fences and items.
    /// Returns ranked results sorted by relevance.
    /// </summary>
    public List<SearchResultItem> Search(string query, SearchScope scope = SearchScope.AllFences, Guid? currentFenceId = null, int currentPageIndex = 0)
    {
        if (string.IsNullOrWhiteSpace(query))
            return new List<SearchResultItem>();

        try
        {
            var results = new List<SearchResultItem>();
            var normalizedQuery = query.ToLowerInvariant();

            var allFences = FenceStateService.Instance.Fences;

            // Filter fences based on scope
            var targetFences = scope switch
            {
                SearchScope.CurrentFence => currentFenceId.HasValue 
                    ? allFences.Where(f => f.Id == currentFenceId.Value).ToList()
                    : new List<FenceModel>(),

                SearchScope.CurrentPage => allFences
                    .Where(f => f.PageIndex == currentPageIndex && !f.TabContainerId.HasValue)
                    .ToList(),

                SearchScope.AllFences => allFences
                    .Where(f => !f.TabContainerId.HasValue) // Exclude tabbed fences
                    .ToList(),

                _ => allFences.ToList()
            };

            // Search items in each fence
            foreach (var fence in targetFences)
            {
                if (fence.Items == null || fence.Items.Count == 0)
                    continue;

                foreach (var item in fence.Items)
                {
                    var score = CalculateRelevance(normalizedQuery, item);
                    
                    if (score > 0)
                    {
                        string effectiveDisplayName = FenceItemResolver.Instance.GetDisplayName(item);
                        string effectivePath = FenceItemResolver.Instance.GetPath(item);

                        results.Add(new SearchResultItem
                        {
                            ItemId = item.Id,
                            FenceId = fence.Id,
                            DisplayName = effectiveDisplayName,
                            TargetPath = effectivePath,
                            FenceTitle = fence.Title,
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
    private double CalculateRelevance(string query, FenceItemModel item)
    {
        double score = 0.0;

        string effectiveDisplayName = FenceItemResolver.Instance.GetDisplayName(item);
        string effectivePath = FenceItemResolver.Instance.GetPath(item);

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
        return Search(query, SearchScope.AllFences);
    }

    /// <summary>
    /// Step 38: Get all items from all fences (for unfiltered browsing).
    /// </summary>
    public List<SearchResultItem> GetAllItems()
    {
        var results = new List<SearchResultItem>();

        try
        {
            var allFences = FenceStateService.Instance.Fences;

            foreach (var fence in allFences.Where(f => !f.TabContainerId.HasValue))
            {
                if (fence.Items == null) continue;

                foreach (var item in fence.Items)
                {
                    results.Add(new SearchResultItem
                    {
                        ItemId = item.Id,
                        FenceId = fence.Id,
                        DisplayName = item.DisplayName,
                        TargetPath = item.TargetPath,
                        FenceTitle = fence.Title,
                        RelevanceScore = 1.0
                    });
                }
            }
        }
        catch { }

        return results;
    }

    /// <summary>
    /// Step 38: Delete an item from a fence by its IDs.
    /// </summary>
    public bool DeleteItem(Guid fenceId, Guid itemId)
    {
        try
        {
            var stateService = FenceStateService.Instance;
            var fence = stateService.GetFence(fenceId);

            if (fence == null || fence.Items == null)
                return false;

            var item = fence.Items.FirstOrDefault(i => i.Id == itemId);
            if (item == null)
                return false;

            fence.Items.Remove(item);
            stateService.MarkDirty();

            Serilog.Log.Information("Item deleted from fence: {ItemId} ← {FenceId}",
                itemId, fenceId);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to delete item {ItemId}", itemId);
            return false;
        }
    }

    /// <summary>
    /// Step 38: Get items from a specific fence.
    /// </summary>
    public List<SearchResultItem> GetFenceItems(Guid fenceId)
    {
        var results = new List<SearchResultItem>();

        try
        {
            var fence = FenceStateService.Instance.GetFence(fenceId);

            if (fence == null || fence.Items == null)
                return results;

            foreach (var item in fence.Items)
            {
                results.Add(new SearchResultItem
                {
                    ItemId = item.Id,
                    FenceId = fence.Id,
                    DisplayName = item.DisplayName,
                    TargetPath = item.TargetPath,
                    FenceTitle = fence.Title,
                    RelevanceScore = 1.0
                });
            }
        }
        catch { }

        return results;
    }
}
