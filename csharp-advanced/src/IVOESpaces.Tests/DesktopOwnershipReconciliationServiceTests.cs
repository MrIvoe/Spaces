using FluentAssertions;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

public class DesktopOwnershipReconciliationServiceTests
{
    [Fact]
    public void EnsureAndReleaseOwnership_TransitionsBetweenDesktopAndSpaceStates()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("transition-item.txt");
        SpaceModel space = CreateSpace(Guid.Parse("10000000-0000-0000-0000-000000000001"), "Standard", SpaceType.Standard);
        SpaceItemModel item = CreateItem(path, displayName: "transition-item");

        harness.OwnershipService.EnsureSpaceItemOwnership(space, item).Should().BeTrue();

        DesktopEntityModel? entity = harness.Registry.TryGetById(item.DesktopEntityId);
        entity.Should().NotBeNull();
        entity!.Ownership.Should().Be(DesktopItemOwnership.SpaceManaged);
        entity.OwnerSpaceId.Should().Be(space.Id);
        item.IsFromDesktop.Should().BeFalse();

        harness.OwnershipService.ReleaseSpaceItemToDesktop(item).Should().BeTrue();

        entity = harness.Registry.TryGetById(item.DesktopEntityId);
        entity.Should().NotBeNull();
        entity!.Ownership.Should().Be(DesktopItemOwnership.DesktopOnly);
        entity.OwnerSpaceId.Should().BeNull();
        item.IsFromDesktop.Should().BeTrue();
    }

    [Fact]
    public void Reconcile_RemovesDuplicatePath_FromLowerPrioritySpace()
    {
        string shared = Path.Combine(Path.GetTempPath(), "shared-item.txt");

        var portal = new SpaceModel
        {
            Id = Guid.NewGuid(),
            Title = "Portal",
            Type = SpaceType.Portal,
            Items = new List<SpaceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = shared, DisplayName = "shared-item", SortOrder = 0 },
            },
        };

        var standard = new SpaceModel
        {
            Id = Guid.NewGuid(),
            Title = "Standard",
            Type = SpaceType.Standard,
            Items = new List<SpaceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = shared, DisplayName = "shared-item", SortOrder = 0 },
            },
        };

        var spaces = new List<SpaceModel> { standard, portal };

        DesktopOwnershipReconciliationService.ReconciliationResult result =
            DesktopOwnershipReconciliationService.Instance.Reconcile(spaces);

        result.RemovedDuplicateItems.Should().Be(1);
        portal.Items.Should().ContainSingle();
        standard.Items.Should().BeEmpty("portal ownership has deterministic priority over standard spaces for identical paths");
        CountVisibleOwnersByPath(spaces)[NormalizePath(shared)].Should().Be(1);
    }

    [Fact]
    public void Reconcile_IsOrderIndependent_ForEquivalentInputSets()
    {
        string pathA = Path.Combine(Path.GetTempPath(), "deterministic-a.txt");
        string pathB = Path.Combine(Path.GetTempPath(), "deterministic-b.txt");

        List<SpaceModel> first = BuildScenario(pathA, pathB, reverse: false);
        List<SpaceModel> second = BuildScenario(pathA, pathB, reverse: true);

        DesktopOwnershipReconciliationService.Instance.Reconcile(first);
        DesktopOwnershipReconciliationService.Instance.Reconcile(second);

        List<string> firstOwners = first.SelectMany(f => f.Items.Select(i => $"{f.Type}:{i.TargetPath}")).OrderBy(s => s).ToList();
        List<string> secondOwners = second.SelectMany(f => f.Items.Select(i => $"{f.Type}:{i.TargetPath}")).OrderBy(s => s).ToList();

        firstOwners.Should().Equal(secondOwners);
    }

    [Fact]
    public void DeleteAndRestoreTransition_DoesNotLeaveResidualSpacePresence()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("delete-restore.txt");
        SpaceModel space = CreateSpace(Guid.Parse("20000000-0000-0000-0000-000000000001"), "Standard", SpaceType.Standard);
        SpaceItemModel item = CreateItem(path, displayName: "delete-restore");
        space.Items.Add(item);

        harness.OwnershipService.EnsureSpaceItemOwnership(space, item).Should().BeTrue();
        harness.OwnershipService.ReleaseSpaceItemToDesktop(item).Should().BeTrue();
        space.Items.Remove(item);

        CountVisibleOwnersByPath(new[] { space }).Should().NotContainKey(NormalizePath(path));
        harness.Registry.TryGetById(item.DesktopEntityId)!.Ownership.Should().Be(DesktopItemOwnership.DesktopOnly);

        SpaceItemModel restored = CreateItem(path, displayName: "delete-restore", entityId: item.DesktopEntityId);
        space.Items.Add(restored);
        harness.OwnershipService.EnsureSpaceItemOwnership(space, restored).Should().BeTrue();

        CountVisibleOwnersByPath(new[] { space })[NormalizePath(path)].Should().Be(1);
        harness.Registry.TryGetById(restored.DesktopEntityId)!.OwnerSpaceId.Should().Be(space.Id);
    }

    [Fact]
    public void DragDropTransition_BetweenStandardSpaces_RemovesTransientDuplicatePresence()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("drag-drop.txt");
        SpaceModel source = CreateSpace(Guid.Parse("30000000-0000-0000-0000-000000000001"), "Source", SpaceType.Standard);
        SpaceModel target = CreateSpace(Guid.Parse("30000000-0000-0000-0000-000000000002"), "Target", SpaceType.Standard);

        SpaceItemModel sourceItem = CreateItem(path, displayName: "drag-drop");
        source.Items.Add(sourceItem);
        harness.OwnershipService.EnsureSpaceItemOwnership(source, sourceItem).Should().BeTrue();

        SpaceItemModel duplicateTargetItem = CreateItem(path, displayName: "drag-drop", entityId: sourceItem.DesktopEntityId);
        target.Items.Add(duplicateTargetItem);

        DesktopOwnershipReconciliationService.ReconciliationResult result =
            DesktopOwnershipReconciliationService.Instance.Reconcile(new[] { target, source });

        result.RemovedDuplicateItems.Should().Be(1);
        CountVisibleOwnersByPath(new[] { source, target })[NormalizePath(path)].Should().Be(1);
        source.Items.Should().ContainSingle();
        target.Items.Should().BeEmpty("duplicate drag/drop overlap between equal-priority spaces must collapse deterministically");
    }

    [Fact]
    public void ReloadTransition_WithPortalAndSpaceDuplicates_RemainsDeterministicAcrossPersistedOrder()
    {
        using var harness = new OwnershipHarness();
        string shared = harness.CreateDesktopFile("reload-shared.txt");
        string other = harness.CreateDesktopFile("reload-other.txt");

        List<SpaceModel> first = BuildReloadScenario(shared, other, reverse: false);
        List<SpaceModel> second = BuildReloadScenario(shared, other, reverse: true);

        DesktopOwnershipReconciliationService.Instance.Reconcile(first);
        DesktopOwnershipReconciliationService.Instance.Reconcile(second);

        SerializeVisibleOwners(first).Should().Equal(SerializeVisibleOwners(second));
        CountVisibleOwnersByPath(first)[NormalizePath(shared)].Should().Be(1);
    }

    private static List<SpaceModel> BuildScenario(string pathA, string pathB, bool reverse)
    {
        var portal = new SpaceModel
        {
            Id = Guid.Parse("11111111-1111-1111-1111-111111111111"),
            Type = SpaceType.Portal,
            Items = new List<SpaceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathA, DisplayName = "A", SortOrder = 0 },
            },
        };

        var standard1 = new SpaceModel
        {
            Id = Guid.Parse("22222222-2222-2222-2222-222222222222"),
            Type = SpaceType.Standard,
            Items = new List<SpaceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathA, DisplayName = "A-duplicate", SortOrder = 0 },
            },
        };

        var standard2 = new SpaceModel
        {
            Id = Guid.Parse("33333333-3333-3333-3333-333333333333"),
            Type = SpaceType.Standard,
            Items = new List<SpaceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathB, DisplayName = "B", SortOrder = 0 },
            },
        };

        return reverse
            ? new List<SpaceModel> { standard2, standard1, portal }
            : new List<SpaceModel> { portal, standard1, standard2 };
    }

    private static List<SpaceModel> BuildReloadScenario(string shared, string other, bool reverse)
    {
        var portal = CreateSpace(Guid.Parse("40000000-0000-0000-0000-000000000001"), "Portal", SpaceType.Portal, CreateItem(shared, "shared-portal"));
        var standard = CreateSpace(Guid.Parse("40000000-0000-0000-0000-000000000002"), "Standard", SpaceType.Standard, CreateItem(shared, "shared-standard"));
        var secondary = CreateSpace(Guid.Parse("40000000-0000-0000-0000-000000000003"), "Secondary", SpaceType.Standard, CreateItem(other, "other-standard"));

        return reverse
            ? new List<SpaceModel> { secondary, standard, portal }
            : new List<SpaceModel> { portal, standard, secondary };
    }

    private static SpaceModel CreateSpace(Guid id, string title, SpaceType type, params SpaceItemModel[] items)
    {
        return new SpaceModel
        {
            Id = id,
            Title = title,
            Type = type,
            Items = items.ToList(),
        };
    }

    private static SpaceItemModel CreateItem(string path, string displayName, Guid? entityId = null)
    {
        return new SpaceItemModel
        {
            Id = Guid.NewGuid(),
            DesktopEntityId = entityId ?? Guid.Empty,
            TargetPath = path,
            DisplayName = displayName,
            IsDirectory = false,
            IsFromDesktop = true,
        };
    }

    private static Dictionary<string, int> CountVisibleOwnersByPath(IEnumerable<SpaceModel> spaces)
    {
        return spaces
            .SelectMany(space => space.Items.Select(item => NormalizePath(item.TargetPath)))
            .GroupBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.Count(), StringComparer.OrdinalIgnoreCase);
    }

    private static List<string> SerializeVisibleOwners(IEnumerable<SpaceModel> spaces)
    {
        return spaces
            .SelectMany(space => space.Items.Select(item => $"{space.Type}:{NormalizePath(item.TargetPath)}"))
            .OrderBy(value => value, StringComparer.Ordinal)
            .ToList();
    }

    private static string NormalizePath(string path)
    {
        return Path.GetFullPath(path).TrimEnd('\\');
    }

    private sealed class OwnershipHarness : IDisposable
    {
        private readonly string _root;

        public OwnershipHarness()
        {
            _root = Path.Combine(Path.GetTempPath(), $"ivoe_ownership_{Guid.NewGuid():N}");
            DesktopDir = Path.Combine(_root, "desktop");
            CommonDesktopDir = Path.Combine(_root, "common-desktop");
            Directory.CreateDirectory(DesktopDir);
            Directory.CreateDirectory(CommonDesktopDir);

            Registry = DesktopEntityRegistryService.CreateForTesting(Path.Combine(_root, "desktop-entities.json"));
            OwnershipService = SpaceFileOwnershipService.CreateForTesting(Registry, DesktopDir, CommonDesktopDir);
        }

        public string DesktopDir { get; }
        public string CommonDesktopDir { get; }
        public DesktopEntityRegistryService Registry { get; }
        public SpaceFileOwnershipService OwnershipService { get; }

        public string CreateDesktopFile(string name)
        {
            string path = Path.Combine(DesktopDir, name);
            File.WriteAllText(path, "test");
            return path;
        }

        public void Dispose()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }
    }
}
