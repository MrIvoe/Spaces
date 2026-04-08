namespace IVOESpaces.Core.Services;

/// <summary>
/// Generic serial async queue primitive with no logging policy or ownership labeling.
/// Prefer higher-level wrappers (for example <c>SerialTaskQueue</c>) when queue intent
/// and failure reporting need to be explicit at call sites.
/// </summary>
public sealed class AsyncSerialQueue
{
    private readonly object _gate = new();
    private Task _tail = Task.CompletedTask;

    public Task Enqueue(Func<Task> operation)
    {
        ArgumentNullException.ThrowIfNull(operation);

        Task prior;
        Task next;

        lock (_gate)
        {
            prior = _tail;
            next = RunAfterAsync(prior, operation);
            _tail = next;
        }

        return next;
    }

    public Task WhenIdleAsync()
    {
        lock (_gate)
            return _tail;
    }

    private static async Task RunAfterAsync(Task prior, Func<Task> operation)
    {
        try
        {
            await prior.ConfigureAwait(false);
        }
        catch
        {
            // Prior failures are already surfaced to their callers; keep queue progression deterministic.
        }

        await operation().ConfigureAwait(false);
    }
}