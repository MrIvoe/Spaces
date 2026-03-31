using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

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
        Register("Toggle All Fences", () =>
        {
            foreach (FenceModel fence in FenceStateService.Instance.Fences)
                fence.IsHidden = !fence.IsHidden;
            FenceStateService.Instance.MarkDirty();
        });

        Register("Apply Dynamic Schedule", () =>
            DynamicFenceScheduler.Instance.ApplyTimeWindowVisibility(DateTime.Now));

        Register("Smart Sort Active Profile", () =>
        {
            foreach (FenceModel fence in FenceStateService.Instance.Fences)
            {
                List<Guid> order = FenceIconGroupingSuggester.Instance.GetSmartSortOrder(fence);
                if (order.Count == 0)
                    continue;

                fence.Items = fence.Items
                    .OrderBy(i => order.IndexOf(i.Id) < 0 ? int.MaxValue : order.IndexOf(i.Id))
                    .ToList();
            }
            FenceStateService.Instance.MarkDirty();
        });
    }
}
