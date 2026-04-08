using IVOESpaces.Core.Plugins;

namespace IVOESpaces.Core.Services;

public sealed class PluginCompatibilityService
{
    private static readonly Lazy<PluginCompatibilityService> _instance = new(() => new PluginCompatibilityService());

    public static PluginCompatibilityService Instance => _instance.Value;

    public string HostVersion { get; } = typeof(PluginCompatibilityService).Assembly.GetName().Version?.ToString(3) ?? "0.0.0";

    private PluginCompatibilityService()
    {
    }

    public PluginCompatibilityResult Evaluate(PluginManifestReader.PluginManifest manifest)
    {
        if (!Version.TryParse(NormalizeVersion(HostVersion), out Version? hostVersion))
            return new PluginCompatibilityResult(false, "Host version unavailable");

        if (!string.IsNullOrWhiteSpace(manifest.MinHostVersion) &&
            Version.TryParse(NormalizeVersion(manifest.MinHostVersion), out Version? min) &&
            hostVersion < min)
        {
            return new PluginCompatibilityResult(false, $"Requires host >= {manifest.MinHostVersion}");
        }

        if (!string.IsNullOrWhiteSpace(manifest.MaxHostVersion) &&
            Version.TryParse(NormalizeVersion(manifest.MaxHostVersion), out Version? max) &&
            hostVersion > max)
        {
            return new PluginCompatibilityResult(false, $"Requires host <= {manifest.MaxHostVersion}");
        }

        return new PluginCompatibilityResult(true, "Compatible");
    }

    private static string NormalizeVersion(string raw)
    {
        if (Version.TryParse(raw, out _))
            return raw;

        string[] parts = raw.Split('.', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 1) return raw + ".0.0";
        if (parts.Length == 2) return raw + ".0";
        return raw;
    }
}
