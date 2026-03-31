using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell;

/// <summary>
/// Serializes asynchronous operations onto a single execution lane.
/// Use <see cref="EnqueueSafe"/> for detached/event-driven entry points where failures should be logged.
/// Use <see cref="Enqueue"/> only when the caller intentionally awaits and handles failures directly.
/// The ownership name is required to make queue intent explicit at every call site.
/// </summary>
internal sealed class SerialTaskQueue
{
    private readonly AsyncSerialQueue _queue = new();

    public Task Enqueue(Func<Task> operation, string ownershipName)
    {
        EnsureOwnershipName(ownershipName);
        return _queue.Enqueue(operation);
    }

    public Task EnqueueSafe(Func<Task> operation, string ownershipName, Action<Exception> onError)
    {
        EnsureOwnershipName(ownershipName);
        return Enqueue(async () =>
        {
            try
            {
                await operation().ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                try
                {
                    onError(ex);
                }
                catch (Exception callbackEx)
                {
                    Log.Error(callbackEx, "SerialTaskQueue: error callback threw while handling queued operation failure ({Ownership})", ownershipName);
                }
            }
        }, ownershipName);
    }

    public Task WhenIdleAsync()
    {
        return _queue.WhenIdleAsync();
    }

    private static void EnsureOwnershipName(string ownershipName)
    {
        if (string.IsNullOrWhiteSpace(ownershipName))
            throw new ArgumentException("Ownership name is required for queue policy enforcement.", nameof(ownershipName));
    }
}