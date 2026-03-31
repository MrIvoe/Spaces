namespace IVOEFences.Shell.Fences;

internal sealed class FenceWindowHost
{
    private readonly Dictionary<Guid, FenceWindow> _windows = new();
    private readonly Dictionary<Guid, FenceRuntimeState> _runtime = new();

    public IReadOnlyCollection<FenceWindow> Windows => _windows.Values;

    public void Add(FenceWindow window)
    {
        _windows[window.ModelId] = window;
        _runtime.TryAdd(window.ModelId, new FenceRuntimeState { FenceId = window.ModelId });
    }

    public void Remove(Guid fenceId)
    {
        _windows.Remove(fenceId);
        _runtime.Remove(fenceId);
    }

    public bool TryGet(Guid fenceId, out FenceWindow? window)
    {
        return _windows.TryGetValue(fenceId, out window);
    }

    public FenceRuntimeState GetRuntime(Guid fenceId)
    {
        if (!_runtime.TryGetValue(fenceId, out FenceRuntimeState? runtime))
        {
            runtime = new FenceRuntimeState { FenceId = fenceId };
            _runtime[fenceId] = runtime;
        }

        return runtime;
    }
}
