using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using System.Reflection;
using Xunit;

namespace IVOEFences.Tests;

public class DragDropTests
{
    [Fact]
    public void ImportIntoFence_WhenConfirmationEnabledAndNoCallback_BlocksDrop()
    {
        var policy = DragDropPolicyService.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        try
        {
            settings.ConfirmExternalDrops = true;
            policy.ConfirmDropCallback = null;

            var fence = new FenceModel { Title = "Target", Type = FenceType.Standard };
            FenceItemModel? imported = policy.ImportIntoFence(fence, @"C:\does-not-matter.txt");

            imported.Should().BeNull();
            fence.Items.Should().BeEmpty();
        }
        finally
        {
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;
        }
    }

    [Fact]
    public void ImportIntoFence_WhenConfirmationDenied_BlocksDrop()
    {
        var policy = DragDropPolicyService.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        try
        {
            settings.ConfirmExternalDrops = true;
            policy.ConfirmDropCallback = (_, _) => false;

            var fence = new FenceModel { Title = "Target", Type = FenceType.Standard };
            FenceItemModel? imported = policy.ImportIntoFence(fence, @"C:\declined.txt");

            imported.Should().BeNull();
            fence.Items.Should().BeEmpty();
        }
        finally
        {
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;
        }
    }

    [Fact]
    public void ImportIntoFence_AutoApplyRules_MovesItemToRuleTargetFence()
    {
        string tempDir = Path.Combine(Path.GetTempPath(), $"ivoe-dragdrop-tests-{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        string filePath = Path.Combine(tempDir, "document.txt");
        File.WriteAllText(filePath, "test");

        var sourceFence = new FenceModel { Id = Guid.NewGuid(), Title = "Source", Type = FenceType.Standard };
        var targetFence = new FenceModel { Id = Guid.NewGuid(), Title = "Target", Type = FenceType.Standard };

        var stateService = FenceStateService.Instance;
        var rulesEngine = SortRulesEngine.Instance;
        AppSettings settings = AppSettingsRepository.Instance.Current;
        var policy = DragDropPolicyService.Instance;

        bool originalAutoApply = settings.AutoApplyRulesOnDrop;
        bool originalConfirm = settings.ConfirmExternalDrops;
        Func<string, string, bool>? originalCallback = policy.ConfirmDropCallback;

        List<FenceModel> originalFences = SnapshotFences(stateService);
        List<SortRulesEngine.SortRule> originalRules = SnapshotRules(rulesEngine);

        try
        {
            ReplaceFences(stateService, new List<FenceModel> { sourceFence, targetFence });
            rulesEngine.ClearRules();
            rulesEngine.AddRule(new SortRulesEngine.SortRule
            {
                Name = "txt->target",
                TargetFenceId = targetFence.Id,
                Type = SortRulesEngine.RuleType.FileExtension,
                Pattern = ".txt",
                Priority = 0,
                Enabled = true,
            });

            settings.AutoApplyRulesOnDrop = true;
            settings.ConfirmExternalDrops = false;
            policy.ConfirmDropCallback = null;

            FenceModel sourceInState = stateService.GetFence(sourceFence.Id)!;
            FenceModel targetInState = stateService.GetFence(targetFence.Id)!;

            FenceItemModel? imported = policy.ImportIntoFence(sourceInState, filePath);

            imported.Should().NotBeNull();
            sourceInState.Items.Should().BeEmpty("rule should move matched drop out of source fence");
            targetInState.Items.Should().ContainSingle(i =>
                string.Equals(i.TargetPath, filePath, StringComparison.OrdinalIgnoreCase));
        }
        finally
        {
            ReplaceFences(stateService, originalFences);
            rulesEngine.ClearRules();
            foreach (SortRulesEngine.SortRule rule in originalRules)
                rulesEngine.AddRule(rule);

            settings.AutoApplyRulesOnDrop = originalAutoApply;
            settings.ConfirmExternalDrops = originalConfirm;
            policy.ConfirmDropCallback = originalCallback;

            Directory.Delete(tempDir, recursive: true);
        }
    }

    private static List<FenceModel> SnapshotFences(FenceStateService service)
    {
        FieldInfo? field = typeof(FenceStateService).GetField("_fences", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();
        var current = field!.GetValue(service) as List<FenceModel>;
        current.Should().NotBeNull();
        return current!.Select(CloneFence).ToList();
    }

    private static void ReplaceFences(FenceStateService service, List<FenceModel> fences)
    {
        FieldInfo? field = typeof(FenceStateService).GetField("_fences", BindingFlags.NonPublic | BindingFlags.Instance);
        field.Should().NotBeNull();
        field!.SetValue(service, fences.Select(CloneFence).ToList());
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
                TargetFenceId = r.TargetFenceId,
                TargetFenceTitle = r.TargetFenceTitle,
                Type = r.Type,
                Pattern = r.Pattern,
                Enabled = r.Enabled,
                Priority = r.Priority,
            })
            .ToList();
    }

    private static FenceModel CloneFence(FenceModel source)
    {
        return new FenceModel
        {
            Id = source.Id,
            Title = source.Title,
            Type = source.Type,
            Items = source.Items.Select(i => new FenceItemModel
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
