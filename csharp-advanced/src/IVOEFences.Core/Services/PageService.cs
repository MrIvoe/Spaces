using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;
using Serilog;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 34: Desktop Pages service for managing multiple pages of fences.
/// Users can organize fences across virtual pages and navigate between them.
/// </summary>
public sealed class PageService
{
    private static PageService? _instance;
    private static readonly object _lock = new();

    private int _currentPageIndex = 0;

    public static PageService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new PageService();
                }
            }
            return _instance;
        }
    }

    public event EventHandler<PageChangedEventArgs>? PageChanged;
    public event EventHandler<PageCountChangedEventArgs>? PageCountChanged;

    public sealed class PageChangedEventArgs : EventArgs
    {
        public int PreviousPageIndex { get; init; }
        public int CurrentPageIndex { get; init; }
    }

    public sealed class PageCountChangedEventArgs : EventArgs
    {
        public int TotalPages { get; init; }
    }

    private PageService()
    {
        var settings = AppSettingsRepository.Instance.Current;
        _currentPageIndex = Math.Max(0, settings.CurrentDesktopPage);
    }

    /// <summary>
    /// Step 34: Gets the current page index (0-based).
    /// </summary>
    public int CurrentPageIndex
    {
        get => _currentPageIndex;
        private set
        {
            int clampedValue = Math.Max(0, Math.Min(value, Math.Max(0, TotalPages - 1)));
            if (_currentPageIndex != clampedValue)
            {
                int previousIndex = _currentPageIndex;
                _currentPageIndex = clampedValue;

                PersistPageSettings();
                
                PageChanged?.Invoke(this, new PageChangedEventArgs
                {
                    PreviousPageIndex = previousIndex,
                    CurrentPageIndex = _currentPageIndex
                });

                Log.Debug("Page changed: {PreviousPage} → {CurrentPage}", 
                    previousIndex + 1, _currentPageIndex + 1);
            }
        }
    }

    /// <summary>
    /// Step 34: Gets the total number of pages (calculated from fences).
    /// </summary>
    public int TotalPages
    {
        get
        {
            try
            {
                var allFences = FenceStateService.Instance.Fences;
                int maxFencePage = allFences.Count == 0 ? 0 : allFences.Max(f => f.PageIndex);
                int configuredPages = Math.Max(1, AppSettingsRepository.Instance.Current.DesktopPageCount);
                return Math.Max(configuredPages, maxFencePage + 1);
            }
            catch
            {
                return 1;
            }
        }
    }

    /// <summary>
    /// Step 34: Currently visible page number (1-based for display).
    /// </summary>
    public int CurrentPageNumber => CurrentPageIndex + 1;

    /// <summary>
    /// Step 34: Navigate to the next page.
    /// </summary>
    public void NextPage()
    {
        CurrentPageIndex = _currentPageIndex + 1;
    }

    /// <summary>
    /// Step 34: Navigate to the previous page.
    /// </summary>
    public void PreviousPage()
    {
        CurrentPageIndex = _currentPageIndex - 1;
    }

    /// <summary>
    /// Step 34: Jump to a specific page (0-based index).
    /// </summary>
    public void GoToPage(int pageIndex)
    {
        CurrentPageIndex = pageIndex;
    }

    /// <summary>
    /// Step 34: Get all fences visible on the current page.
    /// </summary>
    public List<FenceModel> GetFencesForCurrentPage()
    {
        return GetFencesForPage(CurrentPageIndex);
    }

    /// <summary>
    /// Step 34: Get all fences for a specific page (0-based index).
    /// </summary>
    public List<FenceModel> GetFencesForPage(int pageIndex)
    {
        try
        {
            return FenceStateService.Instance.Fences
                .Where(f => f.PageIndex == pageIndex)
                .ToList();
        }
        catch
        {
            return new List<FenceModel>();
        }
    }

    /// <summary>
    /// Step 34: Move a fence to a different page.
    /// Returns true if successful, false if fence not found or move failed.
    /// </summary>
    public bool MoveFenceToPage(Guid fenceId, int targetPageIndex)
    {
        if (targetPageIndex < 0)
            return false;

        try
        {
            var stateService = FenceStateService.Instance;
            var fence = stateService.GetFence(fenceId);

            if (fence == null)
                return false;

            int oldTotalPages = TotalPages;
            int oldPageIndex = fence.PageIndex;
            fence.PageIndex = targetPageIndex;
            EnsureConfiguredPageCount(targetPageIndex + 1);

            stateService.MarkDirty();

            int newTotalPages = TotalPages;

            if (oldTotalPages != newTotalPages)
            {
                PageCountChanged?.Invoke(this, new PageCountChangedEventArgs 
                { 
                    TotalPages = newTotalPages 
                });
            }

            Log.Information("Fence moved: {FenceId} from page {OldPage} to {NewPage}",
                fenceId, oldPageIndex + 1, targetPageIndex + 1);

            return true;
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to move fence {FenceId} to page {TargetPage}", 
                fenceId, targetPageIndex);
            return false;
        }
    }

    /// <summary>
    /// Step 34: Create a new page and return its index (0-based).
    /// </summary>
    public int CreateNewPage()
    {
        try
        {
            int newPageIndex = TotalPages;
            EnsureConfiguredPageCount(newPageIndex + 1);
            Log.Information("New page created: page {PageNumber}", newPageIndex + 1);

            PageCountChanged?.Invoke(this, new PageCountChangedEventArgs 
            { 
                TotalPages = newPageIndex + 1 
            });

            return newPageIndex;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to create new page");
            return -1;
        }
    }

    /// <summary>
    /// Step 34: Delete a page and move its fences to another page.
    /// Returns true if successful.
    /// </summary>
    public bool DeletePage(int pageIndex, int moveRemainderToPage = 0)
    {
        if (pageIndex < 0 || pageIndex >= TotalPages)
            return false;

        if (moveRemainderToPage < 0 || moveRemainderToPage >= TotalPages)
            return false;

        try
        {
            var stateService = FenceStateService.Instance;
            var fencesOnPage = stateService.Fences.Where(f => f.PageIndex == pageIndex).ToList();

            foreach (var fence in fencesOnPage)
            {
                fence.PageIndex = moveRemainderToPage;
            }

            stateService.MarkDirty();
            CompactPages();
            EnsureConfiguredPageCount(Math.Max(1, TotalPages - 1));

            int newTotalPages = TotalPages;
            PageCountChanged?.Invoke(this, new PageCountChangedEventArgs 
            { 
                TotalPages = newTotalPages 
            });

            // If current page was deleted, go to previous page
            if (CurrentPageIndex >= TotalPages)
                CurrentPageIndex = TotalPages - 1;

            Log.Information("Page deleted: page {PageIndex}, {FenceCount} fences moved to page {TargetPage}",
                pageIndex + 1, fencesOnPage.Count, moveRemainderToPage + 1);

            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to delete page {PageIndex}", pageIndex);
            return false;
        }
    }

    /// <summary>
    /// Step 34: Get a list of fence IDs on each page for display.
    /// </summary>
    public Dictionary<int, List<Guid>> GetPageFenceIds()
    {
        var result = new Dictionary<int, List<Guid>>();

        try
        {
            var allFences = FenceStateService.Instance.Fences;
            for (int page = 0; page < TotalPages; page++)
            {
                var fenceIds = allFences
                    .Where(f => f.PageIndex == page)
                    .Select(f => f.Id)
                    .ToList();
                result[page] = fenceIds;
            }
        }
        catch { }

        return result;
    }

    /// <summary>
    /// Step 34: Check if a page is empty (has no fences).
    /// </summary>
    public bool IsPageEmpty(int pageIndex)
    {
        try
        {
            return !FenceStateService.Instance.Fences.Any(f => f.PageIndex == pageIndex);
        }
        catch
        {
            return true;
        }
    }

    /// <summary>
    /// Step 34: Reorder pages by compacting them (remove gaps).
    /// Useful after deleting pages.
    /// </summary>
    public void CompactPages()
    {
        try
        {
            var stateService = FenceStateService.Instance;
            var fencesByPage = stateService.Fences
                .GroupBy(f => f.PageIndex)
                .OrderBy(g => g.Key)
                .ToList();

            int newPageIndex = 0;
            foreach (var group in fencesByPage)
            {
                foreach (var fence in group)
                {
                    fence.PageIndex = newPageIndex;
                }
                newPageIndex++;
            }

            stateService.MarkDirty();
            EnsureConfiguredPageCount(Math.Max(1, fencesByPage.Count));
            Log.Information("Pages compacted: {TotalPages} pages", TotalPages);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to compact pages");
        }
    }

    public void ReloadFromSettings()
    {
        int targetPage = Math.Max(0, AppSettingsRepository.Instance.Current.CurrentDesktopPage);
        CurrentPageIndex = targetPage;
    }

    private static void EnsureConfiguredPageCount(int minimumPages)
    {
        var settings = AppSettingsRepository.Instance.Current;
        int desired = Math.Max(1, minimumPages);
        if (settings.DesktopPageCount == desired)
            return;

        settings.DesktopPageCount = desired;
        SaveSettings();
    }

    private void PersistPageSettings()
    {
        var settings = AppSettingsRepository.Instance.Current;
        settings.CurrentDesktopPage = _currentPageIndex;
        settings.DesktopPageCount = Math.Max(settings.DesktopPageCount, TotalPages);
        SaveSettings();
    }

    private static void SaveSettings()
    {
        AppSettingsRepository.Instance.SaveNow();
    }
}
