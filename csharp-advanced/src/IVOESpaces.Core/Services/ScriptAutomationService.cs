using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public sealed class ScriptAutomationService
{
    private static readonly Lazy<ScriptAutomationService> _instance = new(() => new ScriptAutomationService());
    public static ScriptAutomationService Instance => _instance.Value;

    private readonly Dictionary<string, Action> _scriptHandlers = new(StringComparer.OrdinalIgnoreCase);

    private ScriptAutomationService()
    {
    }

    public void Register(string name, Action handler)
    {
        _scriptHandlers[name] = handler;
    }

    public bool Run(string name)
    {
        if (!AppSettingsRepository.Instance.Current.EnableScriptActions)
            return false;

        if (!_scriptHandlers.TryGetValue(name, out Action? handler))
            return false;

        handler();
        return true;
    }

    public IReadOnlyList<string> GetRegisteredScriptNames() => _scriptHandlers.Keys.OrderBy(n => n).ToList();

    public void RegisterBuiltIns()
    {
        Register("Toggle All Spaces", () =>
        {
            foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
                space.IsHidden = !space.IsHidden;
            SpaceStateService.Instance.MarkDirty();
        });

        Register("Apply Dynamic Schedule", () =>
            DynamicSpaceScheduler.Instance.ApplyTimeWindowVisibility(DateTime.Now));

        Register("Smart Sort Active Profile", () =>
        {
            foreach (SpaceModel space in SpaceStateService.Instance.Spaces)
            {
                List<Guid> order = SpaceIconGroupingSuggester.Instance.GetSmartSortOrder(space);
                if (order.Count == 0)
                    continue;

                space.Items = space.Items
                    .OrderBy(i => order.IndexOf(i.Id) < 0 ? int.MaxValue : order.IndexOf(i.Id))
                    .ToList();
            }
            SpaceStateService.Instance.MarkDirty();
        });
    }
}
