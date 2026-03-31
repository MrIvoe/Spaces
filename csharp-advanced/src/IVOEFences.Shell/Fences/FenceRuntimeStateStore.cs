using IVOEFences.Core.Models;

namespace IVOEFences.Shell.Fences;

/// <summary>
/// Keeps runtime-only fence state separate from persisted fence models.
/// Includes transient UI state and in-flight deletion tracking.
/// </summary>
internal sealed class FenceRuntimeStateStore
{
    private readonly Dictionary<Guid, FenceRuntimeState> _states = new();
    private readonly HashSet<Guid> _deletingFenceIds = new();
    private readonly object _gate = new();

    public FenceRuntimeState GetOrCreate(Guid fenceId)
    {
        lock (_gate)
        {
            if (!_states.TryGetValue(fenceId, out FenceRuntimeState? state))
            {
                state = new FenceRuntimeState { FenceId = fenceId };
                _states[fenceId] = state;
            }

            return state;
        }
    }

    public void RegisterFence(Guid fenceId)
    {
        lock (_gate)
        {
            if (!_states.ContainsKey(fenceId))
                _states[fenceId] = new FenceRuntimeState { FenceId = fenceId };
        }
    }

    public void RemoveFence(Guid fenceId)
    {
        lock (_gate)
        {
            _states.Remove(fenceId);
            _deletingFenceIds.Remove(fenceId);
        }
    }

    public bool TryBeginDelete(Guid fenceId)
    {
        lock (_gate)
        {
            if (_deletingFenceIds.Contains(fenceId))
                return false;

            _deletingFenceIds.Add(fenceId);
            return true;
        }
    }

    public void CompleteDelete(Guid fenceId)
    {
        lock (_gate)
            _deletingFenceIds.Remove(fenceId);
    }

    public bool IsDeleting(Guid fenceId)
    {
        lock (_gate)
            return _deletingFenceIds.Contains(fenceId);
    }

    public void Clear()
    {
        lock (_gate)
        {
            _states.Clear();
            _deletingFenceIds.Clear();
        }
    }
}
