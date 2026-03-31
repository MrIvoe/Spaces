using Serilog;

namespace IVOEFences.Shell.Fences;

internal sealed class FenceUiMutationDispatcher
{
    private sealed record UiMutation(Action Action, string OwnershipName);

    private readonly object _gate = new();
    private readonly Queue<UiMutation> _pendingMutations = new();

    internal int PendingCount
    {
        get
        {
            lock (_gate)
                return _pendingMutations.Count;
        }
    }

    public void Enqueue(Action action, string ownershipName)
    {
        ArgumentNullException.ThrowIfNull(action);
        EnsureOwnershipName(ownershipName);

        lock (_gate)
            _pendingMutations.Enqueue(new UiMutation(action, ownershipName));

        FenceWindow.TryPostDesktopSync();
    }

    public void Drain()
    {
        while (true)
        {
            UiMutation? mutation;
            lock (_gate)
            {
                if (_pendingMutations.Count == 0)
                    break;

                mutation = _pendingMutations.Dequeue();
            }

            try
            {
                mutation.Action();
            }
            catch (Exception ex)
            {
                Log.Error(ex, "FenceUiMutationDispatcher: queued UI mutation failed ({Ownership})", mutation.OwnershipName);
            }
        }
    }

    public void Clear()
    {
        lock (_gate)
            _pendingMutations.Clear();
    }

    private static void EnsureOwnershipName(string ownershipName)
    {
        if (string.IsNullOrWhiteSpace(ownershipName))
            throw new ArgumentException("Ownership name is required for queue policy enforcement.", nameof(ownershipName));
    }
}