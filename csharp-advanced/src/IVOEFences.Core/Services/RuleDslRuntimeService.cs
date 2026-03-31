using System.Text;

namespace IVOEFences.Core.Services;

/// <summary>
/// Loads rules.dsl into SortRulesEngine, resolving fence titles to IDs.
/// If rules.dsl is not present, falls back to persisted rules.json.
/// </summary>
public sealed class RuleDslRuntimeService
{
    private static readonly Lazy<RuleDslRuntimeService> _instance = new(() => new RuleDslRuntimeService());
    public static RuleDslRuntimeService Instance => _instance.Value;

    private readonly RuleDslParser _parser = new();

    private RuleDslRuntimeService()
    {
    }

    public async Task<int> InitializeRulesAsync()
    {
        await SortRulesEngine.Instance.LoadAsync().ConfigureAwait(false);

        if (!File.Exists(AppPaths.RulesDsl))
            return SortRulesEngine.Instance.GetRules().Count;

        string dsl = await File.ReadAllTextAsync(AppPaths.RulesDsl, Encoding.UTF8).ConfigureAwait(false);
        var parsed = _parser.Parse(dsl);

        if (!parsed.IsSuccess)
        {
            foreach (string err in parsed.Errors)
                Serilog.Log.Warning("RuleDslRuntimeService: parse error {Error}", err);
            return SortRulesEngine.Instance.GetRules().Count;
        }

        SortRulesEngine.Instance.ClearRules();
        foreach (var rule in parsed.Rules)
            SortRulesEngine.Instance.AddRule(rule);

        SortRulesEngine.Instance.ResolveFenceTitles(FenceStateService.Instance.Fences);
        await SortRulesEngine.Instance.SaveAsync().ConfigureAwait(false);

        Serilog.Log.Information("RuleDslRuntimeService: loaded {Count} rules from {Path}", parsed.Rules.Count, AppPaths.RulesDsl);
        return parsed.Rules.Count;
    }
}
