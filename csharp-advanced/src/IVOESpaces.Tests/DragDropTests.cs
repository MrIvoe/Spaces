using FluentAssertions;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using System.Reflection;
using Xunit;

namespace IVOESpaces.Tests;

public class DragDropTests
{
    [Fact]
    public void ImportIntoSpace_WhenConfirmationEnabledAndNoCallback_BlocksDrop()
    {
        var policy = DragDropPolicyService.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        try
        {
            settings.ConfirmExternalDrops = true;
            policy.ConfirmDropCallback = null;

            var space = new SpaceModel { Title = "Target", Type = SpaceType.Standard };
            SpaceItemModel? imported = policy.ImportIntoSpace(space, @"C:\does-not-matter.txt");

            imported.Should().BeNull();
            space.Items.Should().BeEmpty();
        }
        finally
        {
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;
        }
    }

    [Fact]
    public void ImportIntoSpace_WhenConfirmationDenied_BlocksDrop()
    {
        var policy = DragDropPolicyService.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        try
        {
            settings.ConfirmExternalDrops = true;
            policy.ConfirmDropCallback = (_, _) => false;

            var space = new SpaceModel { Title = "Target", Type = SpaceType.Standard };
            SpaceItemModel? imported = policy.ImportIntoSpace(space, @"C:\declined.txt");

            imported.Should().BeNull();
            space.Items.Should().BeEmpty();
        }
        finally
        {
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;
        }
    }

    [Fact]
    public void ImportIntoSpace_AutoApplyRules_MovesItemToRuleTargetSpace()
    {
        string tempDir = Path.Combine(Path.GetTempPath(), $"ivoe-dragdrop-tests-{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        string filePath = Path.Combine(tempDir, "document.txt");
        File.WriteAllText(filePath, "test");

        var sourceSpace = new SpaceModel { Id = Guid.NewGuid(), Title = "Source", Type = SpaceType.Standard };
        var targetSpace = new SpaceModel { Id = Guid.NewGuid(), Title = "Target", Type = SpaceType.Standard };

        var stateService = SpaceStateService.Instance;
        var rulesEngine = SortRulesEngine.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        var policy = DragDropPolicyService.Instance;

        bool originalAutoApply = settings.AutoApplyRulesOnDrop;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        List<SpaceModel> originalSpaces = SnapshotSpaces(stateService);
        List<SortRulesEngine.SortRule> originalRules = SnapshotRules(rulesEngine);

        try
        {
            ReplaceSpaces(stateService, new List<SpaceModel> { sourceSpace, targetSpace });
            rulesEngine.ClearRules();
            rulesEngine.AddRule(new SortRulesEngine.SortRule
            {
                Name = "txt->target",
                TargetSpaceId = targetSpace.Id,
                Type = SortRulesEngine.RuleType.FileExtension,
                Pattern = ".txt",
                Priority = 0,
                Enabled = true,
            });

            settings.AutoApplyRulesOnDrop = true;
            settings.ConfirmExternalDrops = false;
            policy.ConfirmDropCallback = null;

            SpaceModel sourceInState = stateService.GetSpace(sourceSpace.Id)!;
            SpaceModel targetInState = stateService.GetSpace(targetSpace.Id)!;

            SpaceItemModel? imported = policy.ImportIntoSpace(sourceInState, filePath);

            imported.Should().NotBeNull();
            sourceInState.Items.Should().BeEmpty("rule should move matched drop out of source space");
            targetInState.Items.Should().ContainSingle(i =>
                string.Equals(i.TargetPath, filePath, StringComparison.OrdinalIgnoreCase));
        }
        finally
        {
            ReplaceSpaces(stateService, originalSpaces);
            rulesEngine.ClearRules();
            foreach (SortRulesEngine.SortRule rule in originalRules)
                rulesEngine.AddRule(rule);

            settings.AutoApplyRulesOnDrop = originalAutoApply;
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;

            Directory.Delete(tempDir, recursive: true);
        }
    }

    private static List<SpaceModel> SnapshotSpaces(SpaceStateService service)
    {
        FieldInfo? field = typeof(SpaceStateService).GetField("_spaces", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();
        var current = field!.GetValue(service) as List<SpaceModel>;
        current.Should().NotBeNull();
        return current!.Select(CloneSpace).ToList();
    }

    private static void ReplaceSpaces(SpaceStateService service, List<SpaceModel> spaces)
    {
        FieldInfo? field = typeof(SpaceStateService).GetField("_spaces", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();
        field!.SetValue(service, spaces.Select(CloneSpace).ToList());
    }

    private static List<SortRulesEngine.SortRule> SnapshotRules(SortRulesEngine engine)
    {
        FieldInfo? field = typeof(SortRulesEngine).GetField("_rules", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();
        var rules = field!.GetValue(engine) as List<SortRulesEngine.SortRule>;
        rules.Should().NotBeNull();
        return rules!
            .Select(r => new SortRulesEngine.SortRule
            {
                Id = r.Id,
                Name = r.Name,
                TargetSpaceId = r.TargetSpaceId,
                TargetSpaceTitle = r.TargetSpaceTitle,
                Type = r.Type,
                Pattern = r.Pattern,
                Enabled = r.Enabled,
                Priority = r.Priority,
            })
            .ToList();
    }

    private static SpaceModel CloneSpace(SpaceModel source)
    {
        return new SpaceModel
        {
            Id = source.Id,
            Title = source.Title,
            Type = source.Type,
            Items = source.Items.Select(i => new SpaceItemModel
            {
                Id = i.Id,
                EntityId = i.EntityId,
                DesktopEntityId = i.DesktopEntityId,
                DisplayName = i.DisplayName,
                TargetPath = i.TargetPath,
                IsDirectory = i.IsDirectory,
                IsFromDesktop = i.IsFromDesktop,
                SortOrder = i.SortOrder,
                TrackedFileType = i.TrackedFileType,
                IsUnresolved = i.IsUnresolved,
            }).ToList(),
        };
    }

}
