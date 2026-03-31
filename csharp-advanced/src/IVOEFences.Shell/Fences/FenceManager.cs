using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using IVOEFences.Shell.AI;
using IVOEFences.Shell.Desktop;
using IVOEFences.Shell.Native;
using Serilog;
using System.Drawing;
using System.IO;

namespace IVOEFences.Shell.Fences;

/// <summary>
/// Owns all <see cref="FenceWindow"/> instances.
/// Loads fence definitions from Core's FenceStateService and creates one Win32
/// window per fence, positioned on the primary monitor work area.
/// </summary>
internal sealed class FenceManager : IDisposable
{
    private static readonly int[] BarThicknessOptions = { 40, 56, 72, 88, 104, 128, 160, 192 };

    private readonly List<FenceWindow> _windows = new();
    private readonly FolderPortalService _portalService = new();
    private readonly AnimationManager _animationManager = new();
    private readonly HoverPreviewManager _hoverManager = new();
    private readonly QuickActionManager _quickActions = new();
    private readonly LayoutManager _layoutManager = new();
    private readonly IconScanner _iconScanner = new();
    private readonly AutoGrouper _autoGrouper = new();
    private readonly AIEnhancedGrouper _aiGrouper = new();
    private readonly FenceAI _fenceAi = new();
    private readonly FenceDesktopSyncCoordinator _desktopSync = new();
    private readonly FenceRulesCoordinator _rulesCoordinator = new();
    private readonly FenceTabCoordinator _tabCoordinator = new();
    private readonly FenceBarModeCoordinator _barCoordinator = new();
    private readonly FenceRuntimeStateStore _runtimeStore = new();
    private readonly FenceLifecycleCoordinator _lifecycle;
    private readonly FenceRestorePlacementPlanner _restorePlacementPlanner = new();
    private readonly QueuedSettingsPersistence _settingsPersistence = new();
    private readonly object _windowGate = new();
    private int _newFenceCounter = 1;

    public bool IsReloadingFromState => _lifecycle.IsReloadingFromState;

    public FenceManager()
    {
        _lifecycle = new FenceLifecycleCoordinator(
            _portalService,
            _runtimeStore,
            _desktopSync,
            SnapshotWindows,
            GetWindowCount,
            InitializeAsync,
            DetachAndDestroyWindow,
            RemoveWindow,
            ClearWindows);

        _portalService.PortalItemsChanged += OnPortalItemsChanged;
        FenceWindow.DesktopSyncRequested += _lifecycle.ProcessPendingDesktopChanges;
        DesktopWatcherService.Instance.ItemCreated += OnDesktopItemCreated;
        DesktopWatcherService.Instance.ItemDeleted += OnDesktopItemDeleted;
        DesktopWatcherService.Instance.ItemRenamed += OnDesktopItemRenamed;

        // Wire live window iteration for Explorer-restart re-anchoring
        DesktopHost.Instance.ReAnchorTargets = () =>
            SnapshotWindows().Where(w => w.IsAlive).Select<FenceWindow, Action<IntPtr>>(w => w.ReAnchorToDesktop);
    }

    private List<FenceWindow> SnapshotWindows()
    {
        lock (_windowGate)
            return _windows.ToList();
    }

    private int GetWindowCount()
    {
        lock (_windowGate)
            return _windows.Count;
    }

    internal int GetPendingDesktopChangeCountForTesting() => _lifecycle.PendingDesktopChangeCount;

    internal int GetPendingUiMutationCountForTesting() => _lifecycle.PendingUiMutationCount;

    internal void SetReloadingFromStateForTesting(bool value) => _lifecycle.SetReloadingFromStateForTesting(value);

    internal bool TryBeginDeleteForTesting(Guid fenceId) => _runtimeStore.TryBeginDelete(fenceId);

    internal bool IsDeletingForTesting(Guid fenceId) => _runtimeStore.IsDeleting(fenceId);

    internal void EnqueueDesktopCreatedForTesting(string path, string? displayName = null) =>
        _lifecycle.EnqueueDesktopCreated(path, displayName ?? Path.GetFileName(path), "test desktop created event");

    internal void EnqueueDesktopDeletedForTesting(string path, string? displayName = null) =>
        _lifecycle.EnqueueDesktopDeleted(path, displayName ?? Path.GetFileName(path), "test desktop deleted event");

    internal void ProcessPendingDesktopChangesForTesting() => _lifecycle.ProcessPendingDesktopChanges();

    internal Task AwaitPendingSettingsTaskAsync() => _settingsPersistence.WhenIdleAsync();

    internal Task AwaitPendingLifecycleTasksAsync() => _lifecycle.AwaitPendingTasksAsync();

    private void DetachAndDestroyWindow(FenceWindow window)
    {
        window.DeleteRequested -= OnWindowDeleteRequested;
        window.IconSizeChanged -= OnWindowIconSizeChanged;
        window.InterFenceSpacingChangeRequested -= OnWindowInterFenceSpacingChangeRequested;
        window.DomainMutationRequested -= OnWindowDomainMutationRequested;
        window.Destroy();
    }

    private void RemoveWindow(FenceWindow window)
    {
        lock (_windowGate)
            _windows.Remove(window);
    }

    private void ClearWindows()
    {
        lock (_windowGate)
            _windows.Clear();
    }

    private FenceWindow? FindWindow(Guid modelId)
    {
        lock (_windowGate)
            return _windows.FirstOrDefault(w => w.ModelId == modelId);
    }

    /// <summary>
    /// Load fences from Core and materialize each as a <see cref="FenceWindow"/>.
    /// </summary>
    public async Task InitializeAsync()
    {
        Log.Information("FenceManager: loading fences from Core");
        var appSettings = AppSettingsRepository.Instance.Current;
        int startupCreated = 0;
        int startupFailed = 0;

        var svc = FenceStateService.Instance;
        await svc.InitializeAsync().ConfigureAwait(false);
        FenceMigrationService.MigrateFenceItemsToEntities(svc.Fences);

        var workArea = DesktopHost.GetPrimaryWorkArea();
        int waWidth  = workArea.right  - workArea.left;
        int waHeight = workArea.bottom - workArea.top;

        if (waWidth <= 0 || waHeight <= 0)
        {
            Log.Warning("FenceManager: work area has zero size — using screen dimensions");
            waWidth  = 1920;
            waHeight = 1080;
        }

        _layoutManager.TryRestoreLayout(svc.Fences, workArea);
        DesktopWatcherService.Instance.Start();
        var activeTabs = BuildActiveTabIndexMap();
        int currentPageIndex = PageService.Instance.CurrentPageIndex;

        foreach (FenceModel model in svc.Fences)
        {
            try
            {
                EnsureFenceOwnsItems(model);

                FenceRestorePlacementPlanner.PlacementPlan placement = _restorePlacementPlanner.Plan(model, workArea);
                Win32.RECT targetArea = placement.TargetArea;
                if (placement.UsedFallbackWorkArea && !string.IsNullOrWhiteSpace(model.MonitorDeviceName))
                {
                    Log.Warning(
                        "FenceManager: monitor work area unavailable for fence '{Title}' on '{Device}' - falling back to primary area",
                        model.Title,
                        model.MonitorDeviceName);
                }

                int x = placement.X;
                int y = placement.Y;
                int w = placement.Width;
                int h = placement.Height;
                bool shouldShowOnCreate = ShouldShowWindowOnCreate(
                    model,
                    appSettings.EnableDesktopPages,
                    currentPageIndex,
                    activeTabs);

                UpdateModelFractionsFromBounds(model, targetArea, x, y, w, h);

                var win = new FenceWindow(model, x, y, w, h);
                bool created = await TryCreateFenceWindowWithRetryAsync(win, shouldShowOnCreate).ConfigureAwait(false);

                if (created && win.IsAlive)
                {
                    startupCreated++;
                    RegisterWindow(win);

                    if (Enum.TryParse(model.SortMode, ignoreCase: true, out FenceWindow.FenceSortMode persistedSort)
                        && persistedSort != FenceWindow.FenceSortMode.Manual)
                    {
                        win.SortIcons(persistedSort);
                    }
                    else if (Enum.TryParse(appSettings.DefaultSortMode, ignoreCase: true, out FenceWindow.FenceSortMode defaultSort)
                        && defaultSort != FenceWindow.FenceSortMode.Manual)
                    {
                        win.SortIcons(defaultSort);
                    }

                    if (model.IsAiSuggested)
                        win.SortIconsByUsage();

                    if (model.IconSizeOverride.HasValue)
                        ResizeFenceToContent(win, Math.Clamp(model.IconSizeOverride.Value, 16, 96), animate: false);

                    // Set opacity directly — no animation on startup.
                    int targetOpacity = Math.Clamp(AppSettingsRepository.Instance.Current.FenceOpacity, 20, 100);
                    win.SetOpacityPercent(targetOpacity);

                    if (model.Type == FenceType.Portal)
                    {
                        if (string.IsNullOrWhiteSpace(model.PortalFolderPath) || !Directory.Exists(model.PortalFolderPath))
                        {
                            Log.Warning("FenceManager: portal fence '{Title}' has missing path '{Path}', showing unavailable state", model.Title, model.PortalFolderPath ?? "<null>");
                        }
                        else
                        {
                            // Attach watcher before enumeration to avoid startup gaps.
                            _portalService.AttachWatcher(model);
                            model.Items = _portalService.EnumeratePortalItems(model).ToList();
                        }
                    }

                    // Always refresh after initialization.
                    win.InvalidateContent();
                }
                else
                {
                    startupFailed++;
                    Log.Warning("FenceManager: failed to materialize fence '{Title}' after retry", model.Title);
                }
            }
            catch (Exception ex)
            {
                startupFailed++;
                Log.Error(ex, "FenceManager: startup restore failed for fence '{Title}'", model.Title);
            }
        }

        if (GetWindowCount() == 0)
        {
            if (appSettings.AutoArrangeOnStartup)
            {
                Log.Information("FenceManager: no saved fences — running standard auto-arrange");
                await AutoArrangeIconsAsync(workArea, useAiGrouping: appSettings.UseAiDefaultMode).ConfigureAwait(false);
            }

            if (GetWindowCount() == 0)
            {
                Log.Information("FenceManager: auto-arrange produced no fences — creating default fence");
                await CreateDefaultAsync(workArea).ConfigureAwait(false);
            }
        }

        int visibleCount = startupCreated;
        Log.Information(
            "FenceManager: startup summary created={Created} failed={Failed} active={Active} visible={Visible}",
            startupCreated,
            startupFailed,
            GetWindowCount(),
            visibleCount);
    }

    private async Task<bool> TryCreateFenceWindowWithRetryAsync(FenceWindow win, bool showOnCreate = true, int retries = 24, int delayMs = 200)
    {
        for (int attempt = 0; attempt < retries; attempt++)
        {
            IntPtr workerW = DesktopHost.Instance.WorkerW;
            if (workerW == IntPtr.Zero)
            {
                Log.Debug("FenceWindow '{Title}': WorkerW not available on attempt {Attempt}/{Retries}", win.Title, attempt + 1, retries);
            }

            win.Create(showOnCreate);
            if (win.IsAlive)
            {
                Log.Information("FenceWindow '{Title}': created successfully on attempt {Attempt} (hwnd={Handle:X})", win.Title, attempt + 1, win.Handle);
                return true;
            }

            if (attempt < retries - 1)
            {
                await Task.Delay(delayMs).ConfigureAwait(false);
                DesktopHost.Instance.OnExplorerRestarted();
            }
        }

        Log.Error("FenceWindow '{Title}': failed to create after {Retries} attempts ({TotalMs}ms)", win.Title, retries, retries * delayMs);
        return false;
    }

    private async Task CreateDefaultAsync(Native.Win32.RECT workArea)
    {
        int waWidth  = Math.Max(workArea.right  - workArea.left, 1);
        int waHeight = Math.Max(workArea.bottom - workArea.top, 1);
        int x = workArea.left + (int)(waWidth  * 0.70);
        int y = workArea.top  + (int)(waHeight * 0.10);
        int w = Math.Max((int)(waWidth  * 0.20), 180);
        int h = Math.Max((int)(waHeight * 0.30), 140);

        var defaultModel = new FenceModel
        {
            Title = "Work",
            XFraction = (double)(x - workArea.left) / waWidth,
            YFraction = (double)(y - workArea.top) / waHeight,
            WidthFraction = (double)w / waWidth,
            HeightFraction = (double)h / waHeight,
        };

        await FenceStateService.Instance.AddFenceAsync(defaultModel).ConfigureAwait(false);

        var win = new FenceWindow(defaultModel, x, y, w, h);
        win.Create();
        if (win.IsAlive)
        {
            RegisterWindow(win);

            int targetOpacity = Math.Clamp(AppSettingsRepository.Instance.Current.FenceOpacity, 20, 100);
            win.SetOpacityPercent(targetOpacity);
        }
    }

    public async Task AutoArrangeIconsAsync(Native.Win32.RECT workArea, bool useAiGrouping = true)
    {
        List<DesktopIcon> icons = _iconScanner.ScanDesktop();
        if (icons.Count == 0)
            return;

        if (workArea.right <= workArea.left || workArea.bottom <= workArea.top)
        {
            Log.Warning("FenceManager.AutoArrange: invalid work area ({L},{T})-({R},{B})", workArea.left, workArea.top, workArea.right, workArea.bottom);
            return;
        }

        Dictionary<string, List<DesktopIcon>> grouped = useAiGrouping
            ? _aiGrouper.GroupIconsByAI(icons)
            : _autoGrouper.GroupIcons(icons);

        var svc = FenceStateService.Instance;
        int waWidth = Math.Max(workArea.right - workArea.left, 1);
        int waHeight = Math.Max(workArea.bottom - workArea.top, 1);

        int cols = Math.Max(1, Math.Min(3, grouped.Count));
        int col = 0;
        int row = 0;
        int spacing = GetInterFenceSpacing();

        foreach ((string title, List<DesktopIcon> groupItems) in grouped.OrderBy(g => g.Key))
        {
            if (groupItems.Count == 0)
                continue;

            int w = Math.Max((int)(waWidth * 0.22), 220);
            int h = Math.Max((int)(waHeight * 0.28), 180);

            int x = workArea.left + 24 + col * (w + spacing);
            int y = workArea.top + 24 + row * (h + spacing);

            if (x + w > workArea.right)
            {
                col = 0;
                row++;
                x = workArea.left + 24;
                y = workArea.top + 24 + row * (h + spacing);
            }

            var model = new FenceModel
            {
                Title = title,
                XFraction = (double)(x - workArea.left) / waWidth,
                YFraction = (double)(y - workArea.top) / waHeight,
                WidthFraction = (double)w / waWidth,
                HeightFraction = (double)h / waHeight,
                Items = groupItems.Select((icon, idx) => new FenceItemModel
                {
                    DesktopEntityId = DesktopEntityRegistryService.Instance.EnsureEntity(icon.FilePath, icon.Name, icon.IsDirectory).Id,
                    DisplayName = icon.Name,
                    TargetPath = icon.FilePath,
                    IsDirectory = icon.IsDirectory,
                    IsFromDesktop = true,
                    SortOrder = idx,
                    TrackedFileType = icon.Extension
                }).ToList()
            };

            EnsureFenceOwnsItems(model);

            await svc.AddFenceAsync(model).ConfigureAwait(false);

            var win = new FenceWindow(model, x, y, w, h);
            win.Create();
            if (win.IsAlive)
            {
                RegisterWindow(win);

                int targetOpacity = Math.Clamp(AppSettingsRepository.Instance.Current.FenceOpacity, 20, 100);
                win.SetOpacityPercent(targetOpacity);
            }

            col++;
            if (col >= cols)
            {
                col = 0;
                row++;
            }
        }
    }

    public Task AutoArrangeIconsAsync(bool useAiGrouping = true)
    {
        return AutoArrangeIconsAsync(DesktopHost.GetPrimaryWorkArea(), useAiGrouping);
    }

    public async Task<int> ApplyAISuggestionsAsync()
    {
        var settings = AppSettingsRepository.Instance.Current;
        var allFences = FenceStateService.Instance.Fences;
        var allItems = allFences.SelectMany(f => f.Items).ToList();
        if (allItems.Count == 0)
            return 0;

        int clusterCount = Math.Clamp(settings.AiSuggestionClusterCount, 1, 6);
        List<List<FenceItemModel>> suggestions = _fenceAi.SuggestFences(allItems, clusterCount);
        if (suggestions.Count == 0)
            return 0;

        Win32.RECT wa = DesktopHost.GetPrimaryWorkArea();
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);
        int iconSize = Math.Clamp(settings.IconSize, 16, 96);
        int spacing = GetInterFenceSpacing();

        int appliedGroups = 0;

        for (int index = 0; index < suggestions.Count; index++)
        {
            List<FenceItemModel> group = suggestions[index];
            if (group.Count == 0)
                continue;

            string title = suggestions.Count == 1 ? "AI Suggested" : $"AI Suggested {index + 1}";
            FenceModel? targetModel = allFences.FirstOrDefault(f => string.Equals(f.Title, title, StringComparison.OrdinalIgnoreCase));
            bool created = false;

            var initialSize = ComputeDynamicFenceSize(group.Count, iconSize, wa);

            if (targetModel == null)
            {
                int w = initialSize.Width;
                int h = initialSize.Height;
                Point origin = ComputeAiFenceOrigin(index, wa, w, h, spacing);
                int x = origin.X;
                int y = origin.Y;

                targetModel = new FenceModel
                {
                    Title = title,
                    IsAiSuggested = true,
                    AiSuggestedAtUtc = DateTime.UtcNow,
                    XFraction = (double)(x - wa.left) / waW,
                    YFraction = (double)(y - wa.top) / waH,
                    WidthFraction = (double)w / waW,
                    HeightFraction = (double)h / waH,
                };

                await FenceStateService.Instance.AddFenceAsync(targetModel).ConfigureAwait(false);
                created = true;

                var targetWindow = new FenceWindow(targetModel, x, y, w, h);
                targetWindow.Create();
                if (targetWindow.IsAlive)
                {
                    RegisterWindow(targetWindow);
                    targetWindow.MarkAsAISuggested();
                }
            }

            var sourceByItem = new Dictionary<Guid, FenceModel>();
            foreach (FenceModel src in allFences)
            {
                foreach (FenceItemModel i in src.Items)
                    sourceByItem[i.Id] = src;
            }

            int moved = 0;
            foreach (FenceItemModel item in group)
            {
                if (targetModel.Items.Any(i => i.Id == item.Id))
                    continue;

                if (!sourceByItem.TryGetValue(item.Id, out FenceModel? srcModel) || srcModel == targetModel)
                    continue;

                srcModel.Items.RemoveAll(i => i.Id == item.Id);
                targetModel.Items.Add(item);
                moved++;
            }

            bool changed = moved > 0 || created;
            if (changed)
            {
                targetModel.IsAiSuggested = true;
                targetModel.AiSuggestedAtUtc = DateTime.UtcNow;

                NormalizeSort(targetModel.Items);
                foreach (FenceModel src in allFences)
                    NormalizeSort(src.Items);

                FenceWindow? targetWindow = FindWindow(targetModel.Id);
                targetWindow?.MarkAsAISuggested();
                targetWindow?.SortIconsByUsage(animate: true);

                if (settings.EnableAiDynamicResizing)
                {
                    var dynamicSize = ComputeDynamicFenceSize(targetModel.Items.Count, iconSize, wa);
                    if (targetWindow != null)
                    {
                        if (targetWindow.Width != dynamicSize.Width || targetWindow.Height != dynamicSize.Height)
                            _ = _animationManager.AnimateResize(targetWindow, new Size(dynamicSize.Width, dynamicSize.Height), 220);

                        targetModel.WidthFraction = (double)dynamicSize.Width / waW;
                        targetModel.HeightFraction = (double)dynamicSize.Height / waH;
                        targetModel.XFraction = (double)(targetWindow.X - wa.left) / waW;
                        targetModel.YFraction = (double)(targetWindow.Y - wa.top) / waH;
                    }
                }

                foreach (FenceWindow w in SnapshotWindows())
                    w.InvalidateContent();

                FenceStateService.Instance.MarkDirty();
                appliedGroups++;
            }
        }

        return appliedGroups;
    }

    private static Size ComputeDynamicFenceSize(int itemCount, int iconSize, Win32.RECT workArea)
    {
        int count = Math.Max(itemCount, 1);
        int padding = 12;
        int titleHeight = 34;
        int cellW = iconSize + padding;
        int cellH = iconSize + 24;

        int cols = Math.Max(1, (int)Math.Ceiling(Math.Sqrt(count)));
        int rows = (int)Math.Ceiling((double)count / cols);

        int idealW = padding * 2 + cols * cellW;
        int idealH = titleHeight + padding + rows * cellH + padding;

        int waW = Math.Max(workArea.right - workArea.left, 1);
        int waH = Math.Max(workArea.bottom - workArea.top, 1);
        int maxW = Math.Max(220, (int)(waW * 0.45));
        int maxH = Math.Max(170, (int)(waH * 0.55));

        return new Size(
            Math.Clamp(idealW, 220, maxW),
            Math.Clamp(idealH, 170, maxH));
    }

    private static Point ComputeAiFenceOrigin(int clusterIndex, Win32.RECT workArea, int width, int height, int spacing)
    {
        spacing = Math.Clamp(spacing, 0, 64);
        int waW = Math.Max(workArea.right - workArea.left, 1);
        int maxCols = Math.Max(1, (waW - 48) / Math.Max(width + spacing, 1));

        int col = clusterIndex % maxCols;
        int row = clusterIndex / maxCols;

        int x = workArea.left + 24 + col * (width + spacing);
        int y = workArea.top + 24 + row * (height + spacing);

        if (x + width > workArea.right)
            x = Math.Max(workArea.left + 12, workArea.right - width - 12);
        if (y + height > workArea.bottom)
            y = Math.Max(workArea.top + 12, workArea.bottom - height - 12);

        return new Point(x, y);
    }

    private static int GetInterFenceSpacing()
    {
        return Math.Clamp(AppSettingsRepository.Instance.Current.InterFenceSpacing, 0, 64);
    }

    private static bool IsNetworkPath(string path)
    {
        return path.StartsWith("\\\\", StringComparison.Ordinal);
    }

    private static void NormalizeSort(List<FenceItemModel> items)
    {
        for (int i = 0; i < items.Count; i++)
            items[i].SortOrder = i;
    }

    public IReadOnlyList<FenceWindow> Windows => SnapshotWindows().AsReadOnly();

    public async Task<CreateFenceResult> CreateFenceAsync(string? title = null)
    {
        Win32.RECT wa = DesktopHost.GetPrimaryWorkArea();
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);
        int iconSize = Math.Clamp(AppSettingsRepository.Instance.Current.IconSize, 16, 96);

        int w = Math.Clamp(300, 220, Math.Max(220, (int)(waW * 0.35)));
        int h = Math.Clamp(220, 170, Math.Max(170, (int)(waH * 0.45)));
        int offset = (_newFenceCounter - 1) * 26;
        int x = Math.Clamp(wa.left + 28 + offset, wa.left + 12, wa.right - w - 12);
        int y = Math.Clamp(wa.top + 28 + offset, wa.top + 12, wa.bottom - h - 12);

        string resolvedTitle = string.IsNullOrWhiteSpace(title)
            ? $"Fence {_newFenceCounter++}"
            : title.Trim();

        var model = new FenceModel
        {
            Title = resolvedTitle,
            XFraction = (double)(x - wa.left) / waW,
            YFraction = (double)(y - wa.top) / waH,
            WidthFraction = (double)w / waW,
            HeightFraction = (double)h / waH,
                PageIndex = AppSettingsRepository.Instance.Current.EnableDesktopPages
                    ? PageService.Instance.CurrentPageIndex
                    : 0,
        };

        await FenceStateService.Instance.AddFenceAsync(model).ConfigureAwait(false);

        var win = new FenceWindow(model, x, y, w, h);
        win.Create();
        if (!win.IsAlive)
            return new CreateFenceResult(model.Id, WindowCreated: false, WindowHandle: IntPtr.Zero);

        RegisterWindow(win);

        // Ensure initial dimensions follow current icon size profile.
        ResizeFenceToContent(win, iconSize, animate: false);

        int targetOpacity = Math.Clamp(AppSettingsRepository.Instance.Current.FenceOpacity, 20, 100);
        win.SetOpacityPercent(targetOpacity);

        return new CreateFenceResult(model.Id, WindowCreated: true, WindowHandle: win.Handle);
    }

    public CreateFenceResult CreateFenceNow(string? title = null)
    {
        Win32.RECT wa = DesktopHost.GetPrimaryWorkArea();
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);
        int iconSize = Math.Clamp(AppSettingsRepository.Instance.Current.IconSize, 16, 96);

        int w = Math.Clamp(300, 220, Math.Max(220, (int)(waW * 0.35)));
        int h = Math.Clamp(220, 170, Math.Max(170, (int)(waH * 0.45)));
        int offset = (_newFenceCounter - 1) * 26;
        int x = Math.Clamp(wa.left + 28 + offset, wa.left + 12, wa.right - w - 12);
        int y = Math.Clamp(wa.top + 28 + offset, wa.top + 12, wa.bottom - h - 12);

        string resolvedTitle = string.IsNullOrWhiteSpace(title)
            ? $"Fence {_newFenceCounter++}"
            : title.Trim();

        var model = new FenceModel
        {
            Title = resolvedTitle,
            XFraction = (double)(x - wa.left) / waW,
            YFraction = (double)(y - wa.top) / waH,
            WidthFraction = (double)w / waW,
            HeightFraction = (double)h / waH,
            PageIndex = AppSettingsRepository.Instance.Current.EnableDesktopPages
                ? PageService.Instance.CurrentPageIndex
                : 0,
        };

        FenceStateService.Instance.AddFenceNow(model);

        var win = new FenceWindow(model, x, y, w, h);
        win.Create();
        if (!win.IsAlive)
            return new CreateFenceResult(model.Id, WindowCreated: false, WindowHandle: IntPtr.Zero);

        RegisterWindow(win);
        ResizeFenceToContent(win, iconSize, animate: false);

        int targetOpacity = Math.Clamp(AppSettingsRepository.Instance.Current.FenceOpacity, 20, 100);
        win.SetOpacityPercent(targetOpacity);

        return new CreateFenceResult(model.Id, WindowCreated: true, WindowHandle: win.Handle);
    }

    public Guid CreateFenceNowId(string? title = null)
    {
        return CreateFenceNow(title).FenceId;
    }

    public async Task<Guid> CreateFenceNowIdAsync(string? title = null)
    {
        return (await CreateFenceAsync(title).ConfigureAwait(false)).FenceId;
    }

    public FenceRuntimeState GetOrCreateRuntimeState(Guid fenceId)
    {
        return _runtimeStore.GetOrCreate(fenceId);
    }

    public void SetGlobalSearchQuery(string query)
    {
        string normalized = query?.Trim() ?? string.Empty;

        foreach (FenceWindow window in SnapshotWindows())
        {
            window.SetSearchQuery(normalized);
            GetOrCreateRuntimeState(window.ModelId).SearchQuery = normalized;
        }
    }

    public void ToggleCollapseExpandAll()
    {
        foreach (FenceWindow window in SnapshotWindows())
            window.ToggleCollapseExpand();
    }

    public void SetAllVisible(bool visible)
    {
        var activeTabs = BuildActiveTabIndexMap();
        foreach (FenceWindow window in SnapshotWindows())
        {
            bool tabVisible = IsWindowVisibleForActiveTab(window, activeTabs);
            bool windowVisible = FenceWindowVisibilityProjector.ShouldBeVisible(
                visible,
                window.IsMarkedHidden(),
                enableDesktopPages: false,
                currentPageIndex: 0,
                windowPageIndex: window.GetPageIndex(),
                tabVisible);
            window.SetVisible(windowVisible);
        }
    }

    public void SetAllOpacityPercent(int percent)
    {
        int clamped = Math.Clamp(percent, 20, 100);
        foreach (FenceWindow window in SnapshotWindows())
            window.SetOpacityPercent(clamped);
    }

    public void ApplyDesktopPageVisibility(bool baseVisible, bool enableDesktopPages, int currentPageIndex)
    {
        var activeTabs = BuildActiveTabIndexMap();
        foreach (FenceWindow window in SnapshotWindows())
        {
            bool tabVisible = IsWindowVisibleForActiveTab(window, activeTabs);
            bool visible = FenceWindowVisibilityProjector.ShouldBeVisible(
                baseVisible,
                window.IsMarkedHidden(),
                enableDesktopPages,
                currentPageIndex,
                window.GetPageIndex(),
                tabVisible);
            window.SetVisible(visible);
        }
    }

    public bool MergeFencesIntoTabGroup(Guid primaryFenceId, Guid secondaryFenceId)
    {
        if (!_tabCoordinator.Merge(primaryFenceId, secondaryFenceId, SnapshotWindows()))
            return false;

        SetAllVisible(true);
        return true;
    }

    public bool SwitchTabForFence(Guid fenceId, int offset)
    {
        if (!_tabCoordinator.Switch(fenceId, offset))
            return false;

        SetAllVisible(true);
        return true;
    }

    public bool DissolveTabGroupForFence(Guid fenceId)
    {
        if (!_tabCoordinator.Dissolve(fenceId, SnapshotWindows()))
            return false;

        SetAllVisible(true);
        return true;
    }

    public string GetTabGroupSummary(Guid fenceId)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        if (model?.TabContainerId == null)
            return "Standalone";

        FenceTabModel? container = TabMergeService.Instance.GetTabContainer(model.TabContainerId.Value);
        List<FenceModel> fences = TabMergeService.Instance.GetFencesInContainer(model.TabContainerId.Value);
        if (container == null || fences.Count == 0)
            return "Standalone";

        return $"Tab {container.ActiveTabIndex + 1}/{fences.Count}";
    }

    public bool ToggleBarMode(Guid fenceId)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        FenceWindow? window = FindWindow(fenceId);
        if (model == null || window == null)
            return false;

        return _barCoordinator.ToggleBarMode(
            window,
            model,
            onDisableBar: () => ResizeFenceToContent(window, window.GetIconSize(), animate: false));
    }

    public bool CycleBarDockEdge(Guid fenceId)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        FenceWindow? window = FindWindow(fenceId);
        if (model == null || window == null)
            return false;

        return _barCoordinator.CycleBarDockEdge(window, model);
    }

    public bool CycleBarThickness(Guid fenceId)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        FenceWindow? window = FindWindow(fenceId);
        if (model == null || window == null)
            return false;

        return _barCoordinator.CycleBarThickness(window, model);
    }

    public string GetBarModeSummary(Guid fenceId)
    {
        FenceModel? model = FenceStateService.Instance.GetFence(fenceId);
        if (model == null)
            return "Unavailable";

        return _barCoordinator.GetBarSummary(model);
    }

    private static Dictionary<Guid, int> BuildActiveTabIndexMap()
    {
        var map = new Dictionary<Guid, int>();
        foreach (FenceTabModel container in TabMergeService.Instance.GetAllTabContainers())
            map[container.Id] = Math.Max(0, container.ActiveTabIndex);
        return map;
    }

    private static bool IsWindowVisibleForActiveTab(FenceWindow window, Dictionary<Guid, int> activeTabs)
    {
        Guid? containerId = window.GetTabContainerId();
        if (!containerId.HasValue)
            return true;

        if (!activeTabs.TryGetValue(containerId.Value, out int activeIndex))
            return true;

        return window.GetTabIndex() == activeIndex;
    }

    internal static bool ShouldShowWindowOnCreate(
        FenceModel model,
        bool enableDesktopPages,
        int currentPageIndex,
        Dictionary<Guid, int> activeTabs)
    {
        bool tabVisible = IsModelVisibleForActiveTab(model, activeTabs);
        bool startupReady = FenceStartupVisibility.IsReadyToShow(model);

        return FenceWindowVisibilityProjector.ShouldBeVisible(
            baseVisible: startupReady,
            isHidden: model.IsHidden,
            enableDesktopPages,
            currentPageIndex,
            model.PageIndex,
            tabVisible);
    }

    private static bool IsModelVisibleForActiveTab(FenceModel model, Dictionary<Guid, int> activeTabs)
    {
        if (!model.TabContainerId.HasValue)
            return true;

        if (!activeTabs.TryGetValue(model.TabContainerId.Value, out int activeIndex))
            return true;

        return model.TabIndex == activeIndex;
    }

    private void ApplyBarLayout(FenceWindow window, FenceModel model)
    {
        Win32.RECT wa = GetFenceWorkArea(window);
        (int x, int y, int w, int h) = ComputeBarBounds(wa, model.DockEdge, model.BarThickness);
        window.SetBounds(x, y, w, h);
        UpdateModelFractionsFromBounds(model, wa, x, y, w, h);
        window.InvalidateContent();
    }

    private static (int x, int y, int w, int h) ComputeBarBounds(Win32.RECT wa, DockEdge edge, int thickness)
    {
        int clampedThickness = Math.Clamp(thickness, 30, 220);
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);

        return edge switch
        {
            DockEdge.Top => (wa.left, wa.top, waW, Math.Min(clampedThickness, waH)),
            DockEdge.Bottom => (wa.left, wa.bottom - Math.Min(clampedThickness, waH), waW, Math.Min(clampedThickness, waH)),
            DockEdge.Left => (wa.left, wa.top, Math.Min(clampedThickness, waW), waH),
            DockEdge.Right => (wa.right - Math.Min(clampedThickness, waW), wa.top, Math.Min(clampedThickness, waW), waH),
            _ => (wa.left, wa.top, waW, Math.Min(clampedThickness, waH)),
        };
    }

    private static void UpdateModelFractionsFromBounds(FenceModel model, Win32.RECT wa, int x, int y, int w, int h)
    {
        int waW = Math.Max(wa.right - wa.left, 1);
        int waH = Math.Max(wa.bottom - wa.top, 1);

        model.XFraction = (double)(x - wa.left) / waW;
        model.YFraction = (double)(y - wa.top) / waH;
        model.WidthFraction = (double)w / waW;
        model.HeightFraction = (double)h / waH;
    }

    private static int NextInt(int current, IReadOnlyList<int> values)
    {
        if (values.Count == 0)
            return current;

        for (int i = 0; i < values.Count; i++)
        {
            if (values[i] > current)
                return values[i];
        }

        return values[0];
    }

    public void SetAllTopmost(bool topmost)
    {
        foreach (FenceWindow window in SnapshotWindows())
            window.SetTopmost(topmost);
    }

    public bool ContainsScreenPoint(int x, int y)
    {
        foreach (FenceWindow window in SnapshotWindows())
        {
            if (!window.IsAlive)
                continue;

            int left = window.X;
            int top = window.Y;
            int right = left + window.Width;
            int bottom = top + window.Height;
            if (x >= left && x < right && y >= top && y < bottom)
                return true;
        }

        return false;
    }

    public void SortAllFences(FenceWindow.FenceSortMode mode)
    {
        foreach (FenceWindow window in SnapshotWindows())
            window.SortIcons(mode, animate: true);
    }

    public void ApplyIconSize(int iconSize)
    {
        int clamped = Math.Clamp(iconSize, 16, 96);
        var settings = AppSettingsRepository.Instance.Current;
        settings.IconSize = clamped;

        foreach (FenceWindow window in SnapshotWindows())
        {
            window.SetIconSize(clamped, notifyManager: false);
            ResizeFenceToContent(window, clamped, animate: true);
            window.InvalidateContent();
        }

        FenceStateService.Instance.MarkDirty();
    }

    public int ApplyAutoPlacementRulesLive()
    {
        return _rulesCoordinator.ApplyAutoPlacementRules(FenceStateService.Instance.Fences, SnapshotWindows());
    }

    public int ApplyFileTypePresetRules()
    {
        var models = FenceStateService.Instance.Fences;
        if (models.Count == 0)
            return 0;

        if (models.Count >= 1)
        {
            models[0].SettingsOverrides.IncludeRules = new List<string> { ".exe", ".lnk", ".url", ".msi", ".bat", ".cmd" };
            models[0].SettingsOverrides.ExcludeRules = new List<string>();
        }

        if (models.Count >= 2)
        {
            models[1].SettingsOverrides.IncludeRules = new List<string> { ".pdf", ".doc", ".docx", ".txt", ".xls", ".xlsx", ".ppt", ".pptx" };
            models[1].SettingsOverrides.ExcludeRules = new List<string>();
        }

        if (models.Count >= 3)
        {
            models[2].SettingsOverrides.IncludeRules = new List<string> { ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg" };
            models[2].SettingsOverrides.ExcludeRules = new List<string>();
        }

        if (models.Count >= 4)
        {
            models[3].SettingsOverrides.IncludeRules = new List<string> { ".zip", ".rar", ".7z", ".mp3", ".wav", ".mp4", ".avi", ".mkv" };
            models[3].SettingsOverrides.ExcludeRules = new List<string>();
        }

        FenceStateService.Instance.MarkDirty();
        return ApplyAutoPlacementRulesLive();
    }

    public int ApplyAlphabeticalPresetRules()
    {
        var models = FenceStateService.Instance.Fences;
        if (models.Count == 0)
            return 0;

        if (models.Count >= 1)
        {
            models[0].SettingsOverrides.IncludeRules = BuildPrefixRules('a', 'h');
            models[0].SettingsOverrides.ExcludeRules = new List<string>();
        }

        if (models.Count >= 2)
        {
            models[1].SettingsOverrides.IncludeRules = BuildPrefixRules('i', 'p');
            models[1].SettingsOverrides.ExcludeRules = new List<string>();
        }

        if (models.Count >= 3)
        {
            models[2].SettingsOverrides.IncludeRules = BuildPrefixRules('q', 'z');
            models[2].SettingsOverrides.ExcludeRules = new List<string>();
        }

        FenceStateService.Instance.MarkDirty();
        return ApplyAutoPlacementRulesLive();
    }

    public int ApplyCatchAllPresetRule(Guid targetFenceId)
    {
        var models = FenceStateService.Instance.Fences;
        var target = models.FirstOrDefault(f => f.Id == targetFenceId);
        if (target == null)
            return 0;

        target.SettingsOverrides.IncludeRules = new List<string> { "*" };
        target.SettingsOverrides.ExcludeRules = new List<string>();
        FenceStateService.Instance.MarkDirty();
        return ApplyAutoPlacementRulesLive();
    }

    private static List<string> BuildPrefixRules(char start, char end)
    {
        var result = new List<string>();
        for (char c = start; c <= end; c++)
            result.Add($"prefix:{c}");
        return result;
    }

    private static void EnsureFenceOwnsItems(FenceModel model)
    {
        bool changed = false;
        foreach (FenceItemModel item in model.Items)
        {
            try
            {
                if (FenceFileOwnershipService.Instance.EnsureFenceItemOwnership(model, item))
                    changed = true;
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "FenceManager: failed to transfer ownership for item '{Path}' in fence '{FenceTitle}'", item.TargetPath, model.Title);
            }
        }

        if (changed)
            FenceStateService.Instance.MarkDirty();
    }

    public void DestroyAll()
    {
        List<FenceWindow> windows = SnapshotWindows();
        _layoutManager.SaveLayout(windows);
        DesktopWatcherService.Instance.Stop();

        foreach (var w in windows)
            _portalService.DetachWatcher(w.ModelId);

        foreach (var w in windows)
            DetachAndDestroyWindow(w);

        ClearWindows();
        _runtimeStore.Clear();
    }

    public void Dispose()
    {
        _portalService.PortalItemsChanged -= OnPortalItemsChanged;
        FenceWindow.DesktopSyncRequested -= _lifecycle.ProcessPendingDesktopChanges;
        DesktopWatcherService.Instance.ItemCreated -= OnDesktopItemCreated;
        DesktopWatcherService.Instance.ItemDeleted -= OnDesktopItemDeleted;
        DesktopWatcherService.Instance.ItemRenamed -= OnDesktopItemRenamed;

        DestroyAll();
    }

    public async Task ReloadFromStateAsync()
    {
        await _lifecycle.ReloadFromStateAsync().ConfigureAwait(false);
    }

    private void RegisterWindow(FenceWindow win)
    {
        lock (_windowGate)
            _windows.Add(win);
        _runtimeStore.RegisterFence(win.ModelId);

        _hoverManager.AttachPreview(win, () =>
            $"Fence: {win.Title}\nItems: {win.GetDisplayedItems().Count}");
        _quickActions.AttachActions(win);
        win.DeleteRequested += OnWindowDeleteRequested;
        win.IconSizeChanged += OnWindowIconSizeChanged;
        win.InterFenceSpacingChangeRequested += OnWindowInterFenceSpacingChangeRequested;
        win.DomainMutationRequested += OnWindowDomainMutationRequested;
        win.SyncDesktopItemVisibility(AppSettingsRepository.Instance.Current.HideDesktopIconsOutsideFences);
    }

    private void OnWindowInterFenceSpacingChangeRequested(int spacing)
    {
        _ = _settingsPersistence.Enqueue(
            () => PersistInterFenceSpacingAsync(spacing),
            ownershipName: "fence inter-spacing persistence");
    }

    private async Task PersistInterFenceSpacingAsync(int spacing)
    {
        AppSettingsRepository.Instance.Current.InterFenceSpacing = spacing;
        try
        {
            await AppSettingsRepository.Instance.SaveAsync().ConfigureAwait(false);
            Log.Information("FenceManager: persisted inter-fence spacing change to {Spacing}px", spacing);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "FenceManager: failed to persist inter-fence spacing change to {Spacing}px", spacing);
        }
    }

    private void OnWindowDomainMutationRequested(FenceWindow window, FenceWindow.FenceDomainMutationKind kind)
    {
        if (_runtimeStore.IsDeleting(window.ModelId))
            return;

        Log.Debug(
            "FenceManager: processing domain mutation {Kind} for fence {FenceId}",
            kind,
            window.ModelId);
        FenceStateService.Instance.MarkDirty();

        if (kind != FenceWindow.FenceDomainMutationKind.OwnershipChanged)
            return;

        DesktopOwnershipReconciliationService.ReconciliationResult reconcile =
            DesktopOwnershipReconciliationService.Instance.Reconcile(FenceStateService.Instance.Fences);
        Log.Information(
            "FenceManager: ownership mutation reconcile removed {Removed} duplicate item(s) and normalized {Normalized} sort order(s)",
            reconcile.RemovedDuplicateItems,
            reconcile.NormalizedSortOrders);

        _desktopSync.ApplyDesktopChangeQueue(SnapshotWindows());
    }

    private void ResizeFenceToContent(FenceWindow window, int iconSize, bool animate)
    {
        Win32.RECT wa = GetFenceWorkArea(window);
        Size target = ComputeDynamicFenceSize(window.GetItemCount(), iconSize, wa);

        int x = Math.Clamp(window.X, wa.left + 8, Math.Max(wa.left + 8, wa.right - target.Width - 8));
        int y = Math.Clamp(window.Y, wa.top + 8, Math.Max(wa.top + 8, wa.bottom - target.Height - 8));

        if (animate)
        {
            if (window.X != x || window.Y != y)
                _ = _animationManager.AnimateMove(window, new Point(x, y), 180);

            if (window.Width != target.Width || window.Height != target.Height)
                _ = _animationManager.AnimateResize(window, target, 200);
            else
                window.SetBounds(x, y, target.Width, target.Height);
        }
        else
        {
            window.SetBounds(x, y, target.Width, target.Height);
        }

        FenceModel? model = FenceStateService.Instance.Fences.FirstOrDefault(f => f.Id == window.ModelId);
        if (model != null)
        {
            int waW = Math.Max(wa.right - wa.left, 1);
            int waH = Math.Max(wa.bottom - wa.top, 1);
            model.XFraction = (double)(x - wa.left) / waW;
            model.YFraction = (double)(y - wa.top) / waH;
            model.WidthFraction = (double)target.Width / waW;
            model.HeightFraction = (double)target.Height / waH;
            model.IconSizeOverride = iconSize;
        }
    }

    private static Win32.RECT GetFenceWorkArea(FenceWindow window)
    {
        if (!window.IsAlive)
            return DesktopHost.GetPrimaryWorkArea();

        IntPtr monitor = Win32.MonitorFromWindow(window.Handle, Win32.MONITOR_DEFAULTTONEAREST);
        var info = new Win32.MONITORINFOEX { cbSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<Win32.MONITORINFOEX>() };
        if (monitor != IntPtr.Zero && Win32.GetMonitorInfo(monitor, ref info))
            return info.rcWork;

        return DesktopHost.GetPrimaryWorkArea();
    }

    private void OnWindowDeleteRequested(FenceWindow window)
    {
        _lifecycle.RequestDelete(window);
    }

    private void OnWindowIconSizeChanged(FenceWindow window, int iconSize)
    {
        ResizeFenceToContent(window, iconSize, animate: true);
        window.InvalidateContent();
        FenceStateService.Instance.MarkDirty();
    }

    private void OnPortalItemsChanged(object? sender, FolderPortalService.PortalItemsChangedEventArgs e)
    {
        _lifecycle.EnqueueUiMutation(
            () => ApplyPortalItemsChanged(e),
            ownershipName: $"portal items changed fence={e.FenceId}");
    }

    private void ApplyPortalItemsChanged(FolderPortalService.PortalItemsChangedEventArgs e)
    {
        if (_runtimeStore.IsDeleting(e.FenceId))
            return;

        FenceModel? model = FenceStateService.Instance.Fences.FirstOrDefault(f => f.Id == e.FenceId);
        if (model == null)
            return;

        model.Items = _portalService.EnumeratePortalItems(model).ToList();
        DesktopOwnershipReconciliationService.ReconciliationResult portalReconcile =
            DesktopOwnershipReconciliationService.Instance.Reconcile(FenceStateService.Instance.Fences);

        if (portalReconcile.RemovedDuplicateItems > 0)
        {
            Log.Information(
                "FenceManager: portal reconcile removed {Removed} duplicate item(s)",
                portalReconcile.RemovedDuplicateItems);
        }

        FenceStateService.Instance.MarkDirty();

        FenceWindow? window = FindWindow(e.FenceId);
        window?.InvalidateContent();
    }

    private void OnDesktopItemCreated(object? sender, DesktopItemEventArgs e)
    {
        _lifecycle.EnqueueDesktopCreated(e.FullPath, e.Item.DisplayName, "desktop watcher created event");
    }

    private void OnDesktopItemDeleted(object? sender, DesktopItemEventArgs e)
    {
        _lifecycle.EnqueueDesktopDeleted(e.FullPath, e.Item.DisplayName, "desktop watcher deleted event");
    }

    private void OnDesktopItemRenamed(object? sender, DesktopItemRenamedEventArgs e)
    {
        _lifecycle.EnqueueDesktopRenamed(e.OldPath, e.NewPath, e.OldName, e.NewName, "desktop watcher renamed event");
    }

}
