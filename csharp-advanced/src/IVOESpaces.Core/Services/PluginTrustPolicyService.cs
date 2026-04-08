using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;
using IVOESpaces.Core.Plugins;

namespace IVOESpaces.Core.Services;

public sealed class PluginTrustPolicyService
{
    private sealed class TrustPolicyDto
    {
        public bool EnforceSignedManifest { get; set; } = true;
        public bool EnforceSignedPackage { get; set; } = true;
        public bool AllowUnsignedLocalDevelopment { get; set; }
        public Dictionary<string, string> TrustedSigningKeys { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    }

    private static readonly Lazy<PluginTrustPolicyService> _instance = new(() => new PluginTrustPolicyService());

    public static PluginTrustPolicyService Instance => _instance.Value;

    public bool EnforceSignedManifest { get; private set; } = true;
    public bool EnforceSignedPackage { get; private set; } = true;
    public bool AllowUnsignedLocalDevelopment { get; private set; }

    private readonly object _lock = new();
    private Dictionary<string, string> _trustedSigningKeys = new(StringComparer.OrdinalIgnoreCase);

    private PluginTrustPolicyService()
    {
        Reload();
    }

    public void Reload()
    {
        lock (_lock)
        {
            EnforceSignedManifest = true;
            EnforceSignedPackage = true;
            AllowUnsignedLocalDevelopment = false;
            _trustedSigningKeys = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            try
            {
                if (!File.Exists(AppPaths.PluginTrustPolicyConfig))
                {
                    SavePolicy_NoLock();
                    return;
                }

                string json = File.ReadAllText(AppPaths.PluginTrustPolicyConfig);
                TrustPolicyDto? dto = JsonSerializer.Deserialize<TrustPolicyDto>(json);
                if (dto == null)
                    return;

                EnforceSignedManifest = dto.EnforceSignedManifest;
                EnforceSignedPackage = dto.EnforceSignedPackage;
                AllowUnsignedLocalDevelopment = dto.AllowUnsignedLocalDevelopment;
                _trustedSigningKeys = dto.TrustedSigningKeys ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            }
            catch
            {
                // Keep defaults if policy cannot be read.
            }
        }
    }

    public IReadOnlyDictionary<string, string> GetTrustedSigningKeys()
    {
        lock (_lock)
        {
            return new Dictionary<string, string>(_trustedSigningKeys, StringComparer.OrdinalIgnoreCase);
        }
    }

    public PluginTrustDecision EvaluateManifest(PluginManifestReader.PluginManifest manifest)
    {
        lock (_lock)
        {
            if (!EnforceSignedManifest)
                return new PluginTrustDecision(true, "Manifest signature enforcement disabled by host policy.");

            if (AllowUnsignedLocalDevelopment && IsLocalManifest(manifest.ManifestPath))
                return new PluginTrustDecision(true, "Unsigned local development plugin allowed by host policy.");

            if (string.IsNullOrWhiteSpace(manifest.SigningKeyId) || string.IsNullOrWhiteSpace(manifest.ManifestSignatureBase64))
                return new PluginTrustDecision(false, "Manifest signature is required by host policy.");

            if (!TryGetPublicKey_NoLock(manifest.SigningKeyId!, out RSA? rsa, out string keyError))
                return new PluginTrustDecision(false, keyError);

            if (rsa == null)
                return new PluginTrustDecision(false, "Signing key is unavailable.");

            using (rsa)
            {
                byte[] payload = Encoding.UTF8.GetBytes(BuildManifestSignaturePayload(manifest));
                byte[] signature;
                try
                {
                    signature = Convert.FromBase64String(manifest.ManifestSignatureBase64!);
                }
                catch
                {
                    return new PluginTrustDecision(false, "Manifest signature is not valid base64.");
                }

                bool ok = rsa.VerifyData(payload, signature, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                return ok
                    ? new PluginTrustDecision(true, "Manifest signature verified.")
                    : new PluginTrustDecision(false, "Manifest signature verification failed.");
            }
        }
    }

    public PluginTrustDecision EvaluatePackageSignature(PluginManifestReader.PluginManifest manifest, string packageChecksumHex)
    {
        lock (_lock)
        {
            if (!EnforceSignedPackage)
                return new PluginTrustDecision(true, "Package signature enforcement disabled by host policy.");

            if (AllowUnsignedLocalDevelopment && IsLocalManifest(manifest.ManifestPath))
                return new PluginTrustDecision(true, "Unsigned local development package allowed by host policy.");

            if (string.IsNullOrWhiteSpace(manifest.SigningKeyId) || string.IsNullOrWhiteSpace(manifest.PackageSignatureBase64))
                return new PluginTrustDecision(false, "Package signature is required by host policy.");

            if (!TryGetPublicKey_NoLock(manifest.SigningKeyId!, out RSA? rsa, out string keyError))
                return new PluginTrustDecision(false, keyError);

            if (rsa == null)
                return new PluginTrustDecision(false, "Signing key is unavailable.");

            using (rsa)
            {
                byte[] payload = Encoding.UTF8.GetBytes(packageChecksumHex.Trim().ToLowerInvariant());
                byte[] signature;
                try
                {
                    signature = Convert.FromBase64String(manifest.PackageSignatureBase64!);
                }
                catch
                {
                    return new PluginTrustDecision(false, "Package signature is not valid base64.");
                }

                bool ok = rsa.VerifyData(payload, signature, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                return ok
                    ? new PluginTrustDecision(true, "Package signature verified.")
                    : new PluginTrustDecision(false, "Package signature verification failed.");
            }
        }
    }

    private static string BuildManifestSignaturePayload(PluginManifestReader.PluginManifest manifest)
    {
        return string.Join("\n", new[]
        {
            manifest.Id,
            manifest.Name,
            manifest.Version,
            manifest.Assembly ?? string.Empty,
            manifest.Entry ?? string.Empty,
            manifest.MinHostVersion ?? string.Empty,
            manifest.MaxHostVersion ?? string.Empty,
            manifest.PackageUrl ?? string.Empty,
            manifest.PackageChecksumSha256 ?? string.Empty,
            manifest.LatestVersion ?? string.Empty,
        });
    }

    private bool TryGetPublicKey_NoLock(string keyId, out RSA? rsa, out string error)
    {
        rsa = null;
        error = string.Empty;

        if (!_trustedSigningKeys.TryGetValue(keyId, out string? pemOrBase64) || string.IsNullOrWhiteSpace(pemOrBase64))
        {
            error = $"Signing key '{keyId}' is not trusted by host policy.";
            return false;
        }

        try
        {
            rsa = RSA.Create();
            if (pemOrBase64.Contains("BEGIN", StringComparison.OrdinalIgnoreCase))
            {
                rsa.ImportFromPem(pemOrBase64);
                return true;
            }

            byte[] spki = Convert.FromBase64String(pemOrBase64);
            rsa.ImportSubjectPublicKeyInfo(spki, out _);
            return true;
        }
        catch
        {
            error = $"Signing key '{keyId}' could not be parsed.";
            return false;
        }
    }

    private static bool IsLocalManifest(string manifestPath)
    {
        string normalized = Path.GetFullPath(manifestPath);
        string appBase = Path.GetFullPath(AppContext.BaseDirectory);
        return normalized.StartsWith(appBase, StringComparison.OrdinalIgnoreCase);
    }

    private void SavePolicy_NoLock()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(AppPaths.PluginTrustPolicyConfig) ?? AppPaths.DataRoot);
        TrustPolicyDto dto = new()
        {
            EnforceSignedManifest = EnforceSignedManifest,
            EnforceSignedPackage = EnforceSignedPackage,
            AllowUnsignedLocalDevelopment = AllowUnsignedLocalDevelopment,
            TrustedSigningKeys = new Dictionary<string, string>(_trustedSigningKeys, StringComparer.OrdinalIgnoreCase),
        };

        string json = JsonSerializer.Serialize(dto, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(AppPaths.PluginTrustPolicyConfig, json);
    }
}
