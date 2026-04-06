namespace IVOEFences.Core.Plugins;

public sealed record PluginCompatibilityResult(
    bool IsCompatible,
    string Reason);
