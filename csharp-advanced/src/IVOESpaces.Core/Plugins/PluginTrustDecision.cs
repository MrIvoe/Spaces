namespace IVOESpaces.Core.Plugins;

public sealed record PluginTrustDecision(
    bool IsTrusted,
    string Reason);
