namespace IVOEFences.Shell.Fences;

/// <summary>
/// Specialized queue wrapper for settings persistence writes.
/// Uses <see cref="SerialTaskQueue"/> so settings-write intent remains explicit
/// and ownership labels are attached by each caller.
/// </summary>
internal sealed class QueuedSettingsPersistence
{
    private readonly SerialTaskQueue _queue = new();

    public Task Enqueue(Func<Task> persistAsync, string ownershipName = "queued settings persistence")
    {
        return _queue.Enqueue(persistAsync, ownershipName);
    }

    public Task WhenIdleAsync() => _queue.WhenIdleAsync();
}