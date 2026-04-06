using System.Reflection;
using System.Text.Json;
using FluentAssertions;
using IVOEFences.Core;
using IVOEFences.Core.Plugins;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class PluginHostWorkflowIntegrationTests
{
    [Fact]
    public void CandidateManifests_InstalledPackagePrecedence_WinsWhenVersionMatches()
    {
        string pluginId = $"workflow.precedence.{Guid.NewGuid():N}";
        string version = "1.2.3";

        string installedDir = Path.Combine(AppPaths.PluginInstalledDir, pluginId, version);
        string legacyRoot = Path.Combine(Path.GetTempPath(), $"ivoe_legacy_plugins_{Guid.NewGuid():N}");
        string legacyDir = Path.Combine(legacyRoot, pluginId);

        Directory.CreateDirectory(installedDir);
        Directory.CreateDirectory(legacyDir);

        try
        {
            WriteManifest(Path.Combine(installedDir, "plugin.json"), pluginId, "Installed Plugin", version, "installed-marker");
            WriteManifest(Path.Combine(legacyDir, "plugin.json"), pluginId, "Legacy Plugin", version, "legacy-marker");

            PluginPackageService packageService = PluginPackageService.Instance;
            PluginManifestReader reader = new();
            IReadOnlyList<PluginManifestReader.PluginManifest> manifests = packageService.GetCandidateManifests(legacyRoot, reader);

            PluginManifestReader.PluginManifest selected = manifests.Single(m => m.Id == pluginId);
            selected.DirectoryPath.Should().Be(installedDir);
            selected.Description.Should().Contain("installed-marker");
        }
        finally
        {
            TryDelete(Path.Combine(AppPaths.PluginInstalledDir, pluginId));
            TryDelete(legacyRoot);
        }
    }

    [Fact]
    public void CompatibilityService_BlocksManifest_WhenHostVersionOutsideRange()
    {
        PluginManifestReader.PluginManifest manifest = new(
            Id: $"workflow.compat.{Guid.NewGuid():N}",
            Name: "Compat Test",
            Version: "1.0.0",
            Description: "compatibility test",
            Author: "test",
            Capabilities: Array.Empty<string>(),
            Entry: null,
            Assembly: "Compat.Plugin.dll",
            DirectoryPath: Path.GetTempPath(),
            ManifestPath: Path.Combine(Path.GetTempPath(), "plugin.json"),
            MinHostVersion: "99.0.0");

            PluginCompatibilityResult result = PluginCompatibilityService.Instance.Evaluate(manifest);

            result.IsCompatible.Should().BeFalse();
            result.Reason.Should().Contain("Requires host >=");
    }

    [Fact]
    public void HostEnabledState_PersistsAcrossStoreReload()
    {
        string pluginId = $"workflow.enabled.{Guid.NewGuid():N}";
        PluginHostSettingsStore.Instance.SetEnabled(pluginId, false);

        ConstructorInfo ctor = typeof(PluginHostSettingsStore)
            .GetConstructor(BindingFlags.Instance | BindingFlags.NonPublic, binder: null, Type.EmptyTypes, modifiers: null)!;

        var reloadedStore = (PluginHostSettingsStore)ctor.Invoke(null);
        bool persisted = reloadedStore.GetEnabled(pluginId, fallback: true);

        persisted.Should().BeFalse();
    }

    private static void WriteManifest(string path, string id, string name, string version, string marker)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var dto = new
        {
            id,
            name,
            version,
            description = $"test manifest {marker}",
            author = "tests",
            capabilities = Array.Empty<string>(),
            assembly = "Sample.Plugin.dll",
        };

        string json = JsonSerializer.Serialize(dto, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(path, json);
    }

    private static void TryDelete(string path)
    {
        try
        {
            if (Directory.Exists(path))
                Directory.Delete(path, recursive: true);
        }
        catch
        {
            // Best effort cleanup for tests.
        }
    }
}
