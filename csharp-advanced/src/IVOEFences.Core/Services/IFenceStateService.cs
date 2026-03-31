using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

/// <summary>
/// Central application state service for fence data.
/// Loads once at startup, maintains in-memory state, persists changes via debounced save.
/// All fence mutations should go through this service instead of direct repository access.
/// </summary>
public interface IFenceStateService
{
    /// <summary>Initialize the state service by loading all fences from disk.</summary>
    Task InitializeAsync();

    /// <summary>Current in-memory fence collection. Always up-to-date.</summary>
    IReadOnlyList<FenceModel> Fences { get; }

    /// <summary>Add a new fence and trigger debounced persist.</summary>
    Task AddFenceAsync(FenceModel fence);

    /// <summary>Update an existing fence in-place and trigger debounced persist.</summary>
    Task UpdateFenceAsync(FenceModel fence);

    /// <summary>Remove a fence by ID and trigger debounced persist.</summary>
    Task RemoveFenceAsync(Guid id);

    /// <summary>Replace the entire fence collection (e.g., snapshot restore).</summary>
    Task ReplaceAllAsync(IEnumerable<FenceModel> fences);

    /// <summary>Force an immediate persist to disk.</summary>
    Task SaveAsync();

    /// <summary>Get a fence by ID, or null if not found.</summary>
    FenceModel? GetFence(Guid id);

    /// <summary>
    /// Mark state as dirty after in-place mutation of fence objects.
    /// Fires StateChanged and schedules a debounced persist.
    /// </summary>
    void MarkDirty();

    /// <summary>Raised after any mutation (add, update, remove, replace).</summary>
    event EventHandler? StateChanged;
}
