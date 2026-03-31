using FluentAssertions;
using IVOEFences.Shell.Fences;
using Xunit;

namespace IVOEFences.Tests;

public class FenceDesktopSyncCoordinatorTests
{
    [Fact]
    public void CreateDesktopItemFromPath_ForFile_UsesRegistryEntityAndExtension()
    {
        using var harness = new DesktopProjectionHarness();
        string path = harness.CreateFile("Quarterly Report.lnk");
        var coordinator = new FenceDesktopSyncCoordinator();

        var item = coordinator.CreateDesktopItemFromPath(path, displayName: null);

        item.Should().NotBeNull();
        item!.TargetPath.Should().Be(path);
        item.DisplayName.Should().Be("Quarterly Report");
        item.IsDirectory.Should().BeFalse();
        item.TrackedFileType.Should().Be(".lnk");
        item.DesktopEntityId.Should().NotBeEmpty();
    }

    [Fact]
    public void CreateDesktopItemFromPath_ForDirectory_UsesFolderProjectionMetadata()
    {
        using var harness = new DesktopProjectionHarness();
        string path = harness.CreateDirectory("Design Assets");
        var coordinator = new FenceDesktopSyncCoordinator();

        var item = coordinator.CreateDesktopItemFromPath(path, displayName: null);

        item.Should().NotBeNull();
        item!.TargetPath.Should().Be(path);
        item.DisplayName.Should().Be("Design Assets");
        item.IsDirectory.Should().BeTrue();
        item.TrackedFileType.Should().Be("folder");
        item.DesktopEntityId.Should().NotBeEmpty();
    }

    private sealed class DesktopProjectionHarness : IDisposable
    {
        private readonly string _root;

        public DesktopProjectionHarness()
        {
            _root = Path.Combine(Path.GetTempPath(), $"ivoe_projection_{Guid.NewGuid():N}");
            Directory.CreateDirectory(_root);
        }

        public string CreateFile(string name)
        {
            string path = Path.Combine(_root, name);
            File.WriteAllText(path, "test");
            return path;
        }

        public string CreateDirectory(string name)
        {
            string path = Path.Combine(_root, name);
            Directory.CreateDirectory(path);
            return path;
        }

        public void Dispose()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }
    }
}