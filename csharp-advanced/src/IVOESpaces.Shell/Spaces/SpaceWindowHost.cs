namespace IVOESpaces.Shell.Spaces;

internal sealed class SpaceWindowHost
{
    private readonly Dictionary<Guid, SpaceWindow> _windows = new();
    private readonly Dictionary<Guid, SpaceRuntimeState> _runtime = new();

    public IReadOnlyCollection<SpaceWindow> Windows => _windows.Values;

    public void Add(SpaceWindow window)
    {
        _windows[window.ModelId] = window;
        _runtime.TryAdd(window.ModelId, new SpaceRuntimeState { SpaceId = window.ModelId });
    }

    public void Remove(Guid spaceId)
    {
        _windows.Remove(spaceId);
        _runtime.Remove(spaceId);
    }

    public bool TryGet(Guid spaceId, out SpaceWindow? window)
    {
        return _windows.TryGetValue(spaceId, out window);
    }

    public SpaceRuntimeState GetRuntime(Guid spaceId)
    {
        if (!_runtime.TryGetValue(spaceId, out SpaceRuntimeState? runtime))
        {
            runtime = new SpaceRuntimeState { SpaceId = spaceId };
            _runtime[spaceId] = runtime;
        }

        return runtime;
    }
}
