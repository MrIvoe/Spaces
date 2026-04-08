using System;
using System.Collections.Generic;
using System.Linq;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Step 35: Service for managing tab container operations.
/// Handles merging spaces into tab groups, adding/removing from containers, etc.
/// </summary>
public sealed class TabMergeService
{
    private static TabMergeService? _instance;
    private static readonly object _lock = new();

    private readonly SpaceRepository _spaceRepository;
    private readonly Dictionary<Guid, SpaceTabModel> _tabContainers = new();

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
        _spaceRepository = SpaceRepository.Instance;
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
            var persistedContainers = _spaceRepository.LoadTabContainers();
            foreach (var container in persistedContainers)
            {
                _tabContainers[container.Id] = container;
            }

            // Also reconstruct from space references for any missing containers
            var allSpaces = SpaceStateService.Instance.Spaces;
            var containerIds = allSpaces
                .Where(f => f.TabContainerId.HasValue)
                .Select(f => f.TabContainerId!.Value)
                .Distinct()
                .Where(id => !_tabContainers.ContainsKey(id))
                .ToList();

            foreach (var containerId in containerIds)
            {
                var spacesInContainer = allSpaces
                    .Where(f => f.TabContainerId == containerId)
                    .OrderBy(f => f.TabIndex)
                    .ToList();

                if (spacesInContainer.Count > 0)
                {
                    var firstSpace = spacesInContainer[0];
                    var container = new SpaceTabModel
                    {
                        Id = containerId,
                        XFraction = firstSpace.XFraction,
                        YFraction = firstSpace.YFraction,
                        WidthFraction = firstSpace.WidthFraction,
                        HeightFraction = firstSpace.HeightFraction,
                        MonitorDeviceName = firstSpace.MonitorDeviceName,
                        PageIndex = firstSpace.PageIndex,
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
    /// Step 35: Create a new tab container from two standalone spaces.
    /// Returns the created container.
    /// </summary>
    public SpaceTabModel? CreateTabContainer(SpaceModel space1, SpaceModel space2)
    {
        if (space1 == null || space2 == null)
            return null;

        if (space1.TabContainerId.HasValue || space2.TabContainerId.HasValue)
        {
            Serilog.Log.Warning("Cannot create tab container: one or both spaces are already in a container");
            return null;
        }

        try
        {
            var containerId = Guid.NewGuid();
            var container = new SpaceTabModel
            {
                Id = containerId,
                XFraction = space1.XFraction,
                YFraction = space1.YFraction,
                WidthFraction = space1.WidthFraction,
                HeightFraction = space1.HeightFraction,
                MonitorDeviceName = space1.MonitorDeviceName,
                PageIndex = space1.PageIndex,
                ActiveTabIndex = 0,
                TabStyle = "Rounded"
            };

            // Assign spaces to container
            space1.TabContainerId = containerId;
            space1.TabIndex = 0;
            space2.TabContainerId = containerId;
            space2.TabIndex = 1;

            // Persist changes
            var stateService = SpaceStateService.Instance;
            var existingSpace1 = stateService.GetSpace(space1.Id);
            var existingSpace2 = stateService.GetSpace(space2.Id);

            if (existingSpace1 != null)
            {
                existingSpace1.TabContainerId = containerId;
                existingSpace1.TabIndex = 0;
            }
            if (existingSpace2 != null)
            {
                existingSpace2.TabContainerId = containerId;
                existingSpace2.TabIndex = 1;
            }

            stateService.MarkDirty();

            // Store container in memory
            _tabContainers[containerId] = container;

            // Persist tab container metadata
            _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = containerId,
                ChangeType = "Created"
            });

            Serilog.Log.Information("Tab container created: {ContainerId} with {Space1} + {Space2}",
                containerId, space1.Id, space2.Id);

            return container;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to create tab container");
            return null;
        }
    }

    /// <summary>
    /// Step 35: Add a standalone space to an existing tab container.
    /// </summary>
    public bool AddSpaceToContainer(SpaceModel space, SpaceTabModel container)
    {
        if (space == null || container == null)
            return false;

        if (space.TabContainerId.HasValue)
        {
            Serilog.Log.Warning("Cannot add space to container: space is already in a container");
            return false;
        }

        try
        {
            // Get next tab index
            var stateService = SpaceStateService.Instance;
            var spacesInContainer = stateService.Spaces
                .Where(f => f.TabContainerId == container.Id)
                .ToList();

            int nextIndex = spacesInContainer.Count;

            // Assign space to container
            space.TabContainerId = container.Id;
            space.TabIndex = nextIndex;

            // Update container position if needed (sync with space position)
            container.LastModifiedAt = DateTime.UtcNow;

            // Persist changes
            var existingSpace = stateService.GetSpace(space.Id);
            if (existingSpace != null)
            {
                existingSpace.TabContainerId = container.Id;
                existingSpace.TabIndex = nextIndex;
            }

            stateService.MarkDirty();
            _tabContainers[container.Id] = container;

            // Persist tab container metadata
            _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
            {
                ContainerId = container.Id,
                ChangeType = "Modified"
            });

            Serilog.Log.Information("Space added to container: {SpaceId} → {ContainerId}",
                space.Id, container.Id);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to add space to container");
            return false;
        }
    }

    /// <summary>
    /// Step 35: Remove a space from its tab container.
    /// If container becomes too small, it may be dissolved.
    /// </summary>
    public bool RemoveSpaceFromContainer(SpaceModel space)
    {
        if (space == null || !space.TabContainerId.HasValue)
            return false;

        try
        {
            var containerId = space.TabContainerId.Value;
            var stateService = SpaceStateService.Instance;

            // Clear space's tab references
            var existingSpace = stateService.GetSpace(space.Id);
            if (existingSpace != null)
            {
                existingSpace.TabContainerId = null;
                existingSpace.TabIndex = 0;
            }
            space.TabContainerId = null;
            space.TabIndex = 0;

            // Check remaining spaces in container
            var remainingSpaces = stateService.Spaces
                .Where(f => f.TabContainerId == containerId)
                .OrderBy(f => f.TabIndex)
                .ToList();

            if (remainingSpaces.Count < 2)
            {
                // Container too small, dissolve it
                foreach (var remainingSpace in remainingSpaces)
                {
                    remainingSpace.TabContainerId = null;
                    remainingSpace.TabIndex = 0;
                }

                _tabContainers.Remove(containerId);
                
                // Persist tab container metadata
                _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

                TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
                {
                    ContainerId = containerId,
                    ChangeType = "Dissolved"
                });

                Serilog.Log.Information("Tab container dissolved: {ContainerId} (fewer than 2 spaces remaining)",
                    containerId);
            }
            else
            {
                // Reindex remaining spaces
                for (int i = 0; i < remainingSpaces.Count; i++)
                {
                    remainingSpaces[i].TabIndex = i;
                }

                TabContainerChanged?.Invoke(this, new TabContainerChangedEventArgs
                {
                    ContainerId = containerId,
                    ChangeType = "Modified"
                });
            }

            stateService.MarkDirty();

            Serilog.Log.Information("Space removed from container: {SpaceId} ← {ContainerId}",
                space.Id, containerId);

            return true;
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to remove space from container");
            return false;
        }
    }

    /// <summary>
    /// Step 35: Get all spaces in a tab container, ordered by TabIndex.
    /// </summary>
    public List<SpaceModel> GetSpacesInContainer(Guid containerId)
    {
        try
        {
            return SpaceStateService.Instance.Spaces
                .Where(f => f.TabContainerId == containerId)
                .OrderBy(f => f.TabIndex)
                .ToList();
        }
        catch
        {
            return new List<SpaceModel>();
        }
    }

    /// <summary>
    /// Step 35: Get a tab container by ID.
    /// </summary>
    public SpaceTabModel? GetTabContainer(Guid containerId)
    {
        _tabContainers.TryGetValue(containerId, out var container);
        return container;
    }

    /// <summary>
    /// Step 35: Get all tab containers.
    /// </summary>
    public List<SpaceTabModel> GetAllTabContainers()
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

        var spaces = GetSpacesInContainer(containerId);
        if (tabIndex < 0 || tabIndex >= spaces.Count)
            return false;

        if (container.ActiveTabIndex == tabIndex)
            return true; // Already active

        try
        {
            container.ActiveTabIndex = tabIndex;
            container.LastModifiedAt = DateTime.UtcNow;
            _tabContainers[containerId] = container;

            // Persist tab container metadata
            _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

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

            // Also update all contained spaces
            var spacesInContainer = SpaceStateService.Instance.Spaces
                .Where(f => f.TabContainerId == containerId)
                .ToList();

            foreach (var space in spacesInContainer)
            {
                space.XFraction = xFrac;
                space.YFraction = yFrac;
                space.WidthFraction = widthFrac;
                space.HeightFraction = heightFrac;
            }

            SpaceStateService.Instance.MarkDirty();

            // Persist tab container metadata
            _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

            Serilog.Log.Debug("Tab container layout updated: {ContainerId}", containerId);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to update tab container layout");
        }
    }

    /// <summary>
    /// Step 35: Completely dissolve a tab container.
    /// All contained spaces become standalone.
    /// </summary>
    public bool DissolveContainer(Guid containerId)
    {
        try
        {
            var stateService = SpaceStateService.Instance;
            var spacesInContainer = stateService.Spaces
                .Where(f => f.TabContainerId == containerId)
                .ToList();

            foreach (var space in spacesInContainer)
            {
                space.TabContainerId = null;
                space.TabIndex = 0;
            }

            stateService.MarkDirty();
            _tabContainers.Remove(containerId);

            // Persist tab container metadata
            _spaceRepository.SaveTabContainers(_tabContainers.Values.ToList());

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
