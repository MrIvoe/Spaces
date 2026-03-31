using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

/// <summary>
/// Integration tests verifying end-to-end behaviour across multiple Core services.
/// Uses real singleton instances; avoid mutating shared state persistently.
/// </summary>
public class IntegrationTests
{
    // ─── SearchService ────────────────────────────────────────────────────────

    [Fact]
    public void SearchService_EmptyQuery_ReturnsEmptyList()
    {
        var results = SearchService.Instance.Search(string.Empty);

        results.Should().BeEmpty("empty queries should never return results");
    }

    [Fact]
    public void SearchService_WhitespaceOnlyQuery_ReturnsEmptyList()
    {
        var results = SearchService.Instance.Search("   \t  ");

        results.Should().BeEmpty();
    }

    [Fact]
    public void SearchService_ValidQueryWithNoFences_ReturnsEmptyList()
    {
        // FenceStateService is not loaded in the test process, so Fences is empty.
        var results = SearchService.Instance.Search("Chrome");

        results.Should().BeEmpty("no fences means no items to match");
    }

    // ─── BehaviorLearningService ─────────────────────────────────────────────

    [Fact]
    public void BehaviorLearning_RecordDrops_FiresSuggestionAtThreshold()
    {
        var service = BehaviorLearningService.Instance;
        var fenceId = Guid.NewGuid(); // unique per test run — no cross-test pollution
        string ext = $".test_{fenceId:N}";

        BehaviorLearningService.RuleSuggestion? captured = null;
        service.RuleSuggested += (_, s) =>
        {
            if (s.Extension == ext && s.FenceId == fenceId)
                captured = s;
        };

        for (int i = 0; i < BehaviorLearningService.SuggestionThreshold; i++)
            service.RecordItemDroppedToFence(ext, fenceId, "TestFence");

        captured.Should().NotBeNull("suggestion should fire exactly at the threshold");
        captured!.MoveCount.Should().Be(BehaviorLearningService.SuggestionThreshold);
        captured.FenceTitle.Should().Be("TestFence");
    }

    [Fact]
    public void BehaviorLearning_ExtensionNormalised_UpperAndLowerMatch()
    {
        var service = BehaviorLearningService.Instance;
        var fenceId = Guid.NewGuid();
        string extBase = $".mix_{Guid.NewGuid():N}";
        string upperExt = extBase.ToUpperInvariant();
        string lowerExt = extBase.ToLowerInvariant();

        // Record 2 with upper-case → before threshold
        service.RecordItemDroppedToFence(upperExt, fenceId, "Docs");
        service.RecordItemDroppedToFence(upperExt, fenceId, "Docs");

        // GetPendingSuggestions should find nothing yet (need 3)
        var pending = service.GetPendingSuggestions();
        pending.Should().NotContain(s => s.FenceId == fenceId);

        // Third with lower-case — normalisation must make them the same extension
        service.RecordItemDroppedToFence(lowerExt, fenceId, "Docs");

        pending = service.GetPendingSuggestions();
        pending.Should().Contain(s => s.FenceId == fenceId && string.Equals(s.Extension, lowerExt, StringComparison.Ordinal),
            "upper and lower-case extensions should be treated as the same key");
    }

    [Fact]
    public void BehaviorLearning_EmptyExtension_Ignored()
    {
        // Must not throw and must not change observable state.
        var before = BehaviorLearningService.Instance.GetPendingSuggestions().Count;

        BehaviorLearningService.Instance.RecordItemDroppedToFence(string.Empty, Guid.NewGuid(), "Fence");
        BehaviorLearningService.Instance.RecordItemDroppedToFence("   ", Guid.NewGuid(), "Fence");

        BehaviorLearningService.Instance.GetPendingSuggestions().Count.Should().Be(before);
    }

    // ─── FencePortalSyncManager ───────────────────────────────────────────────

    [Fact]
    public void FencePortalSyncManager_RegisterAndUnregister_TracksWatchState()
    {
        var tempDir = Path.Combine(Path.GetTempPath(), $"ivoe_portal_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        try
        {
            var fence = new FenceModel
            {
                Id    = Guid.NewGuid(),
                Title = "PortalIntegrationTest",
                Type  = FenceType.Portal,
                PortalFolderPath = tempDir,
            };

            var mgr = FencePortalSyncManager.Instance;

            mgr.IsPortalFenceWatched(fence.Id).Should().BeFalse("fence not yet registered");

            mgr.RegisterPortalFence(fence, tempDir);
            mgr.IsPortalFenceWatched(fence.Id).Should().BeTrue("fence should be watched after registration");

            mgr.UnregisterPortalFence(fence.Id);
            mgr.IsPortalFenceWatched(fence.Id).Should().BeFalse("fence should no longer be watched after removal");
        }
        finally
        {
            Directory.Delete(tempDir, recursive: true);
        }
    }

    [Fact]
    public void FencePortalSyncManager_RegisterWithMissingFolder_DoesNotWatch()
    {
        var fence = new FenceModel
        {
            Id    = Guid.NewGuid(),
            Title = "MissingFolderTest",
        };
        string nonExistentPath = Path.Combine(Path.GetTempPath(), $"no_such_dir_{Guid.NewGuid():N}");

        FencePortalSyncManager.Instance.RegisterPortalFence(fence, nonExistentPath);

        FencePortalSyncManager.Instance.IsPortalFenceWatched(fence.Id)
            .Should().BeFalse("cannot watch a folder that does not exist");
    }

    // ─── DynamicFenceScheduler ────────────────────────────────────────────────

    [Fact]
    public void DynamicFenceScheduler_ApplyTimeWindow_DoesNotThrowWhenNoFences()
    {
        // FenceStateService.Fences is empty in the test process; the scheduler
        // must handle this gracefully with no exception.
        var act = () => DynamicFenceScheduler.Instance.ApplyTimeWindowVisibility(DateTime.Now);
        act.Should().NotThrow();
    }

    // ─── ThemeEngine ─────────────────────────────────────────────────────────

    [Fact]
    public void ThemeEngine_GetColorPalette_ReturnsNonEmptyDictionary()
    {
        var palette = ThemeEngine.Instance.GetColorPalette();

        palette.Should().NotBeEmpty("the theme engine must always provide at least a baseline palette");
    }

    [Fact]
    public void ThemeEngine_ForceDarkMode_IsDarkModeReturnsTrue()
    {
        var engine = ThemeEngine.Instance;
        var previousMode = engine.CurrentThemeMode;
        try
        {
            engine.CurrentThemeMode = ThemeEngine.ThemeMode.Dark;
            engine.IsDarkMode.Should().BeTrue();
        }
        finally
        {
            engine.CurrentThemeMode = previousMode;
        }
    }

    [Fact]
    public void ThemeEngine_ForceLightMode_IsDarkModeReturnsFalse()
    {
        var engine = ThemeEngine.Instance;
        var previousMode = engine.CurrentThemeMode;
        try
        {
            engine.CurrentThemeMode = ThemeEngine.ThemeMode.Light;
            engine.IsDarkMode.Should().BeFalse();
        }
        finally
        {
            engine.CurrentThemeMode = previousMode;
        }
    }
}
