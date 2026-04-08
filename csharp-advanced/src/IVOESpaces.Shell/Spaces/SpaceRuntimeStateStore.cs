using IVOESpaces.Core.Models;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Keeps runtime-only space state separate from persisted space models.
/// Includes transient UI state and in-flight deletion tracking.
/// </summary>
internal sealed class SpaceRuntimeStateStore
{
    private readonly Dictionary<Guid, SpaceRuntimeState> _states = new();
    private readonly HashSet<Guid> _deletingSpaceIds = new();
    private readonly object _gate = new();

    public SpaceRuntimeState GetOrCreate(Guid spaceId)
    {
        lock (_gate)
        {
            if (!_states.TryGetValue(spaceId, out SpaceRuntimeState? state))
            {
                state = new SpaceRuntimeState { SpaceId = spaceId };
                _states[spaceId] = state;
            }

            return state;
        }
    }

    public void RegisterSpace(Guid spaceId)
    {
        lock (_gate)
        {
            if (!_states.ContainsKey(spaceId))
                _states[spaceId] = new SpaceRuntimeState { SpaceId = spaceId };
        }
    }

    public void RemoveSpace(Guid spaceId)
    {
        lock (_gate)
        {
            _states.Remove(spaceId);
            _deletingSpaceIds.Remove(spaceId);
        }
    }

    public bool TryBeginDelete(Guid spaceId)
    {
        lock (_gate)
        {
            if (_deletingSpaceIds.Contains(spaceId))
                return false;

            _deletingSpaceIds.Add(spaceId);
            return true;
        }
    }

    public void CompleteDelete(Guid spaceId)
    {
        lock (_gate)
            _deletingSpaceIds.Remove(spaceId);
    }

    public bool IsDeleting(Guid spaceId)
    {
        lock (_gate)
            return _deletingSpaceIds.Contains(spaceId);
    }

    public void Clear()
    {
        lock (_gate)
        {
            _states.Clear();
            _deletingSpaceIds.Clear();
        }
    }
}
