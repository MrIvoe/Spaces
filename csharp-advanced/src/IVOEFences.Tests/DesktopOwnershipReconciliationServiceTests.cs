using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

public class DesktopOwnershipReconciliationServiceTests
{
    [Fact]
    public void EnsureAndReleaseOwnership_TransitionsBetweenDesktopAndFenceStates()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("transition-item.txt");
        FenceModel fence = CreateFence(Guid.Parse("10000000-0000-0000-0000-000000000001"), "Standard", FenceType.Standard);
        FenceItemModel item = CreateItem(path, displayName: "transition-item");

        harness.OwnershipService.EnsureFenceItemOwnership(fence, item).Should().BeTrue();

        DesktopEntityModel? entity = harness.Registry.TryGetById(item.DesktopEntityId);
        entity.Should().NotBeNull();
        entity!.Ownership.Should().Be(DesktopItemOwnership.FenceManaged);
        entity.OwnerFenceId.Should().Be(fence.Id);
        item.IsFromDesktop.Should().BeFalse();

        harness.OwnershipService.ReleaseFenceItemToDesktop(item).Should().BeTrue();

        entity = harness.Registry.TryGetById(item.DesktopEntityId);
        entity.Should().NotBeNull();
        entity!.Ownership.Should().Be(DesktopItemOwnership.DesktopOnly);
        entity.OwnerFenceId.Should().BeNull();
        item.IsFromDesktop.Should().BeTrue();
    }

    [Fact]
    public void Reconcile_RemovesDuplicatePath_FromLowerPriorityFence()
    {
        string shared = Path.Combine(Path.GetTempPath(), "shared-item.txt");

        var portal = new FenceModel
        {
            Id = Guid.NewGuid(),
            Title = "Portal",
            Type = FenceType.Portal,
            Items = new List<FenceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = shared, DisplayName = "shared-item", SortOrder = 0 },
            },
        };

        var standard = new FenceModel
        {
            Id = Guid.NewGuid(),
            Title = "Standard",
            Type = FenceType.Standard,
            Items = new List<FenceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = shared, DisplayName = "shared-item", SortOrder = 0 },
            },
        };

        var fences = new List<FenceModel> { standard, portal };

        DesktopOwnershipReconciliationService.ReconciliationResult result =
            DesktopOwnershipReconciliationService.Instance.Reconcile(fences);

        result.RemovedDuplicateItems.Should().Be(1);
        portal.Items.Should().ContainSingle();
        standard.Items.Should().BeEmpty("portal ownership has deterministic priority over standard fences for identical paths");
        CountVisibleOwnersByPath(fences)[NormalizePath(shared)].Should().Be(1);
    }

    [Fact]
    public void Reconcile_IsOrderIndependent_ForEquivalentInputSets()
    {
        string pathA = Path.Combine(Path.GetTempPath(), "deterministic-a.txt");
        string pathB = Path.Combine(Path.GetTempPath(), "deterministic-b.txt");

        List<FenceModel> first = BuildScenario(pathA, pathB, reverse: false);
        List<FenceModel> second = BuildScenario(pathA, pathB, reverse: true);

        DesktopOwnershipReconciliationService.Instance.Reconcile(first);
        DesktopOwnershipReconciliationService.Instance.Reconcile(second);

        List<string> firstOwners = first.SelectMany(f => f.Items.Select(i => $"{f.Type}:{i.TargetPath}")).OrderBy(s => s).ToList();
        List<string> secondOwners = second.SelectMany(f => f.Items.Select(i => $"{f.Type}:{i.TargetPath}")).OrderBy(s => s).ToList();

        firstOwners.Should().Equal(secondOwners);
    }

    [Fact]
    public void DeleteAndRestoreTransition_DoesNotLeaveResidualFencePresence()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("delete-restore.txt");
        FenceModel fence = CreateFence(Guid.Parse("20000000-0000-0000-0000-000000000001"), "Standard", FenceType.Standard);
        FenceItemModel item = CreateItem(path, displayName: "delete-restore");
        fence.Items.Add(item);

        harness.OwnershipService.EnsureFenceItemOwnership(fence, item).Should().BeTrue();
        harness.OwnershipService.ReleaseFenceItemToDesktop(item).Should().BeTrue();
        fence.Items.Remove(item);

        CountVisibleOwnersByPath(new[] { fence }).Should().NotContainKey(NormalizePath(path));
        harness.Registry.TryGetById(item.DesktopEntityId)!.Ownership.Should().Be(DesktopItemOwnership.DesktopOnly);

        FenceItemModel restored = CreateItem(path, displayName: "delete-restore", entityId: item.DesktopEntityId);
        fence.Items.Add(restored);
        harness.OwnershipService.EnsureFenceItemOwnership(fence, restored).Should().BeTrue();

        CountVisibleOwnersByPath(new[] { fence })[NormalizePath(path)].Should().Be(1);
        harness.Registry.TryGetById(restored.DesktopEntityId)!.OwnerFenceId.Should().Be(fence.Id);
    }

    [Fact]
    public void DragDropTransition_BetweenStandardFences_RemovesTransientDuplicatePresence()
    {
        using var harness = new OwnershipHarness();
        string path = harness.CreateDesktopFile("drag-drop.txt");
        FenceModel source = CreateFence(Guid.Parse("30000000-0000-0000-0000-000000000001"), "Source", FenceType.Standard);
        FenceModel target = CreateFence(Guid.Parse("30000000-0000-0000-0000-000000000002"), "Target", FenceType.Standard);

        FenceItemModel sourceItem = CreateItem(path, displayName: "drag-drop");
        source.Items.Add(sourceItem);
        harness.OwnershipService.EnsureFenceItemOwnership(source, sourceItem).Should().BeTrue();

        FenceItemModel duplicateTargetItem = CreateItem(path, displayName: "drag-drop", entityId: sourceItem.DesktopEntityId);
        target.Items.Add(duplicateTargetItem);

        DesktopOwnershipReconciliationService.ReconciliationResult result =
            DesktopOwnershipReconciliationService.Instance.Reconcile(new[] { target, source });

        result.RemovedDuplicateItems.Should().Be(1);
        CountVisibleOwnersByPath(new[] { source, target })[NormalizePath(path)].Should().Be(1);
        source.Items.Should().ContainSingle();
        target.Items.Should().BeEmpty("duplicate drag/drop overlap between equal-priority fences must collapse deterministically");
    }

    [Fact]
    public void ReloadTransition_WithPortalAndFenceDuplicates_RemainsDeterministicAcrossPersistedOrder()
    {
        using var harness = new OwnershipHarness();
        string shared = harness.CreateDesktopFile("reload-shared.txt");
        string other = harness.CreateDesktopFile("reload-other.txt");

        List<FenceModel> first = BuildReloadScenario(shared, other, reverse: false);
        List<FenceModel> second = BuildReloadScenario(shared, other, reverse: true);

        DesktopOwnershipReconciliationService.Instance.Reconcile(first);
        DesktopOwnershipReconciliationService.Instance.Reconcile(second);

        SerializeVisibleOwners(first).Should().Equal(SerializeVisibleOwners(second));
        CountVisibleOwnersByPath(first)[NormalizePath(shared)].Should().Be(1);
    }

    private static List<FenceModel> BuildScenario(string pathA, string pathB, bool reverse)
    {
        var portal = new FenceModel
        {
            Id = Guid.Parse("11111111-1111-1111-1111-111111111111"),
            Type = FenceType.Portal,
            Items = new List<FenceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathA, DisplayName = "A", SortOrder = 0 },
            },
        };

        var standard1 = new FenceModel
        {
            Id = Guid.Parse("22222222-2222-2222-2222-222222222222"),
            Type = FenceType.Standard,
            Items = new List<FenceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathA, DisplayName = "A-duplicate", SortOrder = 0 },
            },
        };

        var standard2 = new FenceModel
        {
            Id = Guid.Parse("33333333-3333-3333-3333-333333333333"),
            Type = FenceType.Standard,
            Items = new List<FenceItemModel>
            {
                new() { Id = Guid.NewGuid(), TargetPath = pathB, DisplayName = "B", SortOrder = 0 },
            },
        };

        return reverse
            ? new List<FenceModel> { standard2, standard1, portal }
            : new List<FenceModel> { portal, standard1, standard2 };
    }

    private static List<FenceModel> BuildReloadScenario(string shared, string other, bool reverse)
    {
        var portal = CreateFence(Guid.Parse("40000000-0000-0000-0000-000000000001"), "Portal", FenceType.Portal, CreateItem(shared, "shared-portal"));
        var standard = CreateFence(Guid.Parse("40000000-0000-0000-0000-000000000002"), "Standard", FenceType.Standard, CreateItem(shared, "shared-standard"));
        var secondary = CreateFence(Guid.Parse("40000000-0000-0000-0000-000000000003"), "Secondary", FenceType.Standard, CreateItem(other, "other-standard"));

        return reverse
            ? new List<FenceModel> { secondary, standard, portal }
            : new List<FenceModel> { portal, standard, secondary };
    }

    private static FenceModel CreateFence(Guid id, string title, FenceType type, params FenceItemModel[] items)
    {
        return new FenceModel
        {
            Id = id,
            Title = title,
            Type = type,
            Items = items.ToList(),
        };
    }

    private static FenceItemModel CreateItem(string path, string displayName, Guid? entityId = null)
    {
        return new FenceItemModel
        {
            Id = Guid.NewGuid(),
            DesktopEntityId = entityId ?? Guid.Empty,
            TargetPath = path,
            DisplayName = displayName,
            IsDirectory = false,
            IsFromDesktop = true,
        };
    }

    private static Dictionary<string, int> CountVisibleOwnersByPath(IEnumerable<FenceModel> fences)
    {
        return fences
            .SelectMany(fence => fence.Items.Select(item => NormalizePath(item.TargetPath)))
            .GroupBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.Count(), StringComparer.OrdinalIgnoreCase);
    }

    private static List<string> SerializeVisibleOwners(IEnumerable<FenceModel> fences)
    {
        return fences
            .SelectMany(fence => fence.Items.Select(item => $"{fence.Type}:{NormalizePath(item.TargetPath)}"))
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
            OwnershipService = FenceFileOwnershipService.CreateForTesting(Registry, DesktopDir, CommonDesktopDir);
        }

        public string DesktopDir { get; }
        public string CommonDesktopDir { get; }
        public DesktopEntityRegistryService Registry { get; }
        public FenceFileOwnershipService OwnershipService { get; }

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
