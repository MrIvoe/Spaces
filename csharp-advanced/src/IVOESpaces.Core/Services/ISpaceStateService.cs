using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Central application state service for space data.
/// Loads once at startup, maintains in-memory state, persists changes via debounced save.
/// All space mutations should go through this service instead of direct repository access.
/// </summary>
public interface ISpaceStateService
{
    /// <summary>Initialize the state service by loading all spaces from disk.</summary>
    Task InitializeAsync();

    /// <summary>Current in-memory space collection. Always up-to-date.</summary>
    IReadOnlyList<SpaceModel> Spaces { get; }

    /// <summary>Add a new space and trigger debounced persist.</summary>
    Task AddSpaceAsync(SpaceModel space);

    /// <summary>Update an existing space in-place and trigger debounced persist.</summary>
    Task UpdateSpaceAsync(SpaceModel space);

    /// <summary>Remove a space by ID and trigger debounced persist.</summary>
    Task RemoveSpaceAsync(Guid id);

    /// <summary>Replace the entire space collection (e.g., snapshot restore).</summary>
    Task ReplaceAllAsync(IEnumerable<SpaceModel> spaces);

    /// <summary>Force an immediate persist to disk.</summary>
    Task SaveAsync();

    /// <summary>Get a space by ID, or null if not found.</summary>
    SpaceModel? GetSpace(Guid id);

    /// <summary>
    /// Mark state as dirty after in-place mutation of space objects.
    /// Fires StateChanged and schedules a debounced persist.
    /// </summary>
    void MarkDirty();

    /// <summary>Raised after any mutation (add, update, remove, replace).</summary>
    event EventHandler? StateChanged;
}
