namespace IVOEFences.Core.Plugins;

public sealed record PluginTrustDecision(
    bool IsTrusted,
    string Reason);
