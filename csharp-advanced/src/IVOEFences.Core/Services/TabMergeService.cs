using System;
using System.Collections.Generic;
using System.Linq;
using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Step 35: Service for managing tab container operations.
/// Handles merging fences into tab groups, adding/removing from containers, etc.
/// </summary>
public sealed class TabMergeService
{
    private static TabMergeService? _instance;
    private static readonly object _lock = new();

    private readonly FenceRepository _fenceRepository;
    private readonly Dictionary<Guid, FenceTabModel> _tabContainers = new();

    public static TabMergeService Instance
    {
        get
        {
            if (_instance == null)
            {
                lock (_lock)
                {
                    if (_instance == null)
                        _instance = new TabMergeService();
                }
            }
            return _instance;
        }
    }

    public event EventHandler<TabContainerChangedEventArgs>? TabContainerChanged;

    public sealed class TabContainerChangedEventArgs : EventArgs
    {
        public Guid ContainerId { get; init; }
        public string ChangeType { get; init; } = string.Empty; // "Created", "Modified", "Dissolved"
    }

    private TabMergeService()
    {
        _fenceRepository = FenceRepository.Instance;
        LoadTabContainers();
    }

    /// <summary>
    /// Step 35: Load all tab containers from the repository.
    /// Called on service initialization.
    /// </summary>
    private void LoadTabContainers()
    {
        try
        {
            // Load persisted tab container metadata
            var persistedContainers = _fenceRepository.LoadTabContainers();
            foreach (var container in persistedContainers)
            {
                _tabContainers[container.Id] = container;
            }

            // Also reconstruct from fence references for any missing containers
            var allFences = FenceStateService.Instance.Fences;
            var containerIds = allFences
                .Where(f => f.TabContainerId.HasValue)
                .Select(f => f.TabContainerId!.Value)
                .Distinct()
                .Where(id => !_tabContainers.ContainsKey(id))
                .ToList();

            foreach (var containerId in containerIds)
            {
                var fencesInContainer = allFences
                    .Where(f => f.TabContainerId == containerId)
                    .OrderBy(f => f.TabIndex)
                    .ToList();

                if (fencesInContainer.Count > 0)
                {
                    var firstFence = fencesInContainer[0];
                    var container = new FenceTabModel
                    {
                        Id = containerId,
                        XFraction = firstFence.XFraction,
                        YFraction = firstFence.YFraction,
                        WidthFraction = firstFence.WidthFraction,
                        HeightFraction = firstFence.HeightFraction,
                        MonitorDeviceName = firstFence.MonitorDeviceName,
                        PageIndex = firstFence.PageIndex,
                        ActiveTabIndex = 0
                    };
                    _tabContainers[containerId] = container;
                }
            }

            Serilog.Log.Information("Loaded {ContainerCount} tab containers", _tabContainers.Count);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to load tab containers");
        }
    }

    /// <summary>
    /// Step 35: Create a new tab container from two standalone fences.
    /// Returns the created container.
    /// </summary>
    public FenceTabModel? CreateTabContainer(FenceModel fence1, FenceModel fence2)
    {
        if (fence1 == null || fence2 == null)
            return null;

        if (fence1.TabContainerId.HasValue || fence2.TabContainerId.HasValue)
        {
            Serilog.Log.Warning("Cannot create tab container: one or both fences are already in a container");
            return null;
        }

        try
        {
            var containerId = Guid.NewGuid();
            var container = new FenceTabModel
            {
                Id = containerId,
                XFraction = fence1.XFraction,
                YFraction = fence1.YFraction,
                WidthFraction = fence1.WidthFraction,
                HeightFraction = fence1.HeightFraction,
                MonitorDeviceName = fence1.MonitorDeviceName,
                PageIndex = fence1.PageIndex,
                ActiveTabIndex = 0,
                TabStyle = "Rounded"
            };

            // Assign fences to container
            fence1.TabContainerId = containerId;
            fence1.TabIndex = 0;
            fence2.TabContainerId = containerId;
            fence2.TabIndex = 1;

            // Persist changes
            var stateService = FenceStateService.Instance;
            var existingFence1 = stateService.GetFence(fence1.Id);
            var existingFence2 = stateService.GetFence(fence2.Id);

            if (existingFence1 != null)
            {
                existingFence1.TabContainerId = containerId;
                existingFence1.TabIndex = 0;
            }
            if (existingFence2 != null)
            {
                existingFence2.TabContainerId = containerId;
                existingFence2.TabIndex = 1;
            }

            stateService.MarkDirty();

            // Store container in memory
            _tabContainers[containerId] = container;

            // Persist tab container metadata
            _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = containerId,
                ChangeType = "Created"
            });

            Serilog.Log.Information("Tab container created: {ContainerId} with {Fence1} + {Fence2}",
                containerId, fence1.Id, fence2.Id);

            return container;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to create tab container");
            return null;
        }
    }

    /// <summary>
    /// Step 35: Add a standalone fence to an existing tab container.
    /// </summary>
    public bool AddFenceToContainer(FenceModel fence, FenceTabModel container)
    {
        if (fence == null || container == null)
            return false;

        if (fence.TabContainerId.HasValue)
        {
            Serilog.Log.Warning("Cannot add fence to container: fence is already in a container");
            return false;
        }

        try
        {
            // Get next tab index
            var stateService = FenceStateService.Instance;
            var fencesInContainer = stateService.Fences
                .Where(f => f.TabContainerId == container.Id)
                .ToList();

            int nextIndex = fencesInContainer.Count;

            // Assign fence to container
            fence.TabContainerId = container.Id;
            fence.TabIndex = nextIndex;

            // Update container position if needed (sync with fence position)
            container.LastModifiedAt = DateTime.UtcNow;

            // Persist changes
            var existingFence = stateService.GetFence(fence.Id);
            if (existingFence != null)
            {
                existingFence.TabContainerId = container.Id;
                existingFence.TabIndex = nextIndex;
            }

            stateService.MarkDirty();
            _tabContainers[container.Id] = container;

            // Persist tab container metadata
            _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = container.Id,
                ChangeType = "Modified"
            });

            Serilog.Log.Information("Fence added to container: {FenceId} → {ContainerId}",
                fence.Id, container.Id);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to add fence to container");
            return false;
        }
    }

    /// <summary>
    /// Step 35: Remove a fence from its tab container.
    /// If container becomes too small, it may be dissolved.
    /// </summary>
    public bool RemoveFenceFromContainer(FenceModel fence)
    {
        if (fence == null || !fence.TabContainerId.HasValue)
            return false;

        try
        {
            var containerId = fence.TabContainerId.Value;
            var stateService = FenceStateService.Instance;

            // Clear fence's tab references
            var existingFence = stateService.GetFence(fence.Id);
            if (existingFence != null)
            {
                existingFence.TabContainerId = null;
                existingFence.TabIndex = 0;
            }
            fence.TabContainerId = null;
            fence.TabIndex = 0;

            // Check remaining fences in container
            var remainingFences = stateService.Fences
                .Where(f => f.TabContainerId == containerId)
                .OrderBy(f => f.TabIndex)
                .ToList();

            if (remainingFences.Count < 2)
            {
                // Container too small, dissolve it
                foreach (var remainingFence in remainingFences)
                {
                    remainingFence.TabContainerId = null;
                    remainingFence.TabIndex = 0;
                }

                _tabContainers.Remove(containerId);
                
                // Persist tab container metadata
                _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

                TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
                {
                    ContainerId = containerId,
                    ChangeType = "Dissolved"
                });

                Serilog.Log.Information("Tab container dissolved: {ContainerId} (fewer than 2 fences remaining)",
                    containerId);
            }
            else
            {
                // Reindex remaining fences
                for (int i = 0; i < remainingFences.Count; i++)
                {
                    remainingFences[i].TabIndex = i;
                }

                TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
                {
                    ContainerId = containerId,
                    ChangeType = "Modified"
                });
            }

            stateService.MarkDirty();

            Serilog.Log.Information("Fence removed from container: {FenceId} ← {ContainerId}",
                fence.Id, containerId);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to remove fence from container");
            return false;
        }
    }

    /// <summary>
    /// Step 35: Get all fences in a tab container, ordered by TabIndex.
    /// </summary>
    public List<FenceModel> GetFencesInContainer(Guid containerId)
    {
        try
        {
            return FenceStateService.Instance.Fences
                .Where(f => f.TabContainerId == containerId)
                .OrderBy(f => f.TabIndex)
                .ToList();
        }
        catch
        {
            return new List<FenceModel>();
        }
    }

    /// <summary>
    /// Step 35: Get a tab container by ID.
    /// </summary>
    public FenceTabModel? GetTabContainer(Guid containerId)
    {
        _tabContainers.TryGetValue(containerId, out var container);
        return container;
    }

    /// <summary>
    /// Step 35: Get all tab containers.
    /// </summary>
    public List<FenceTabModel> GetAllTabContainers()
    {
        return _tabContainers.Values.ToList();
    }

    /// <summary>
    /// Step 35: Switch the active tab in a container.
    /// </summary>
    public bool SwitchActiveTab(Guid containerId, int tabIndex)
    {
        if (!_tabContainers.TryGetValue(containerId, out var container))
            return false;

        var fences = GetFencesInContainer(containerId);
        if (tabIndex < 0 || tabIndex >= fences.Count)
            return false;

        if (container.ActiveTabIndex == tabIndex)
            return true; // Already active

        try
        {
            container.ActiveTabIndex = tabIndex;
            container.LastModifiedAt = DateTime.UtcNow;
            _tabContainers[containerId] = container;

            // Persist tab container metadata
            _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = containerId,
                ChangeType = "Modified"
            });

            Serilog.Log.Debug("Tab switched in container {ContainerId}: → index {TabIndex}",
                containerId, tabIndex);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to switch active tab");
            return false;
        }
    }

    /// <summary>
    /// Step 35: Update tab container position/size.
    /// </summary>
    public void UpdateTabContainerLayout(Guid containerId, double xFrac, double yFrac, double widthFrac, double heightFrac)
    {
        if (!_tabContainers.TryGetValue(containerId, out var container))
            return;

        try
        {
            container.XFraction = xFrac;
            container.YFraction = yFrac;
            container.WidthFraction = widthFrac;
            container.HeightFraction = heightFrac;
            container.LastModifiedAt = DateTime.UtcNow;
            _tabContainers[containerId] = container;

            // Also update all contained fences
            var fencesInContainer = FenceStateService.Instance.Fences
                .Where(f => f.TabContainerId == containerId)
                .ToList();

            foreach (var fence in fencesInContainer)
            {
                fence.XFraction = xFrac;
                fence.YFraction = yFrac;
                fence.WidthFraction = widthFrac;
                fence.HeightFraction = heightFrac;
            }

            FenceStateService.Instance.MarkDirty();

            // Persist tab container metadata
            _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            Serilog.Log.Debug("Tab container layout updated: {ContainerId}", containerId);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to update tab container layout");
        }
    }

    /// <summary>
    /// Step 35: Completely dissolve a tab container.
    /// All contained fences become standalone.
    /// </summary>
    public bool DissolveContainer(Guid containerId)
    {
        try
        {
            var stateService = FenceStateService.Instance;
            var fencesInContainer = stateService.Fences
                .Where(f => f.TabContainerId == containerId)
                .ToList();

            foreach (var fence in fencesInContainer)
            {
                fence.TabContainerId = null;
                fence.TabIndex = 0;
            }

            stateService.MarkDirty();
            _tabContainers.Remove(containerId);

            // Persist tab container metadata
            _fenceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = containerId,
                ChangeType = "Dissolved"
            });

            Serilog.Log.Information("Tab container completely dissolved: {ContainerId}", containerId);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to dissolve tab container");
            return false;
        }
    }
}
