using FluentAssertions;
using IVOESpaces.Core;
using Xunit;

namespace IVOESpaces.Tests;

public class AppPathsTests
{
    [Fact]
    public void SettingsAndSpacePaths_AreUnderDataRoot()
    {
        AppPaths.SettingsConfig.StartsWith(AppPaths.DataRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
        AppPaths.SpacesConfig.StartsWith(AppPaths.DataRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
        AppPaths.RulesConfig.StartsWith(AppPaths.DataRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
        AppPaths.BehaviorLog.StartsWith(AppPaths.DataRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
    }

    [Fact]
    public void WorkspaceAndStoragePaths_AreUnderUserRoot()
    {
        AppPaths.WorkspaceRoot.StartsWith(AppPaths.UserRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
        AppPaths.StorageRoot.StartsWith(AppPaths.UserRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
        AppPaths.RulesDsl.StartsWith(AppPaths.UserRoot, StringComparison.OrdinalIgnoreCase).Should().BeTrue();
    }

    [Fact]
    public void EnsureDirectories_CreatesRequiredDirectories()
    {
        AppPaths.EnsureDirectories();

        Directory.Exists(AppPaths.DataRoot).Should().BeTrue();
        Directory.Exists(AppPaths.UserRoot).Should().BeTrue();
        Directory.Exists(AppPaths.WorkspaceRoot).Should().BeTrue();
        Directory.Exists(AppPaths.StorageRoot).Should().BeTrue();
        Directory.Exists(AppPaths.SnapshotsDir).Should().BeTrue();
        Directory.Exists(AppPaths.ThemesDir).Should().BeTrue();
        Directory.Exists(AppPaths.LogsDir).Should().BeTrue();
        Directory.Exists(AppPaths.PluginsDir).Should().BeTrue();
    }
}
