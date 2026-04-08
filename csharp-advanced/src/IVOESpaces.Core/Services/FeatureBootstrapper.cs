namespace IVOESpaces.Core.Services;

/// <summary>
/// Initializes optional advanced feature modules in a single place.
/// This keeps shell startup deterministic while enabling feature toggles.
/// </summary>
public static class FeatureBootstrapper
{
    private static bool _initialized;

    public static void Initialize()
    {
        if (_initialized)
            return;

        ScriptAutomationService.Instance.RegisterBuiltIns();

        // Register command palette entries for built-in scripts.
        foreach (string script in ScriptAutomationService.Instance.GetRegisteredScriptNames())
        {
            CommandPaletteService.Instance.Register(new Models.CommandPaletteEntry
            {
                Id = $"script:{script}",
                Type = Models.CommandPaletteEntryType.Script,
                Title = script,
                Subtitle = "Built-in script",
                Score = 0.5,
                Execute = () => ScriptAutomationService.Instance.Run(script)
            });
        }

        _initialized = true;
    }
}
