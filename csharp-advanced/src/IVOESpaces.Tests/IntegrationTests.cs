using FluentAssertions;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

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
    public void SearchService_ValidQueryWithNoSpaces_ReturnsEmptyList()
    {
        // SpaceStateService is not loaded in the test process, so Spaces is empty.
        var results = SearchService.Instance.Search("Chrome");

        results.Should().BeEmpty("no spaces means no items to match");
    }

    // ─── BehaviorLearningService ─────────────────────────────────────────────

    [Fact]
    public void BehaviorLearning_RecordDrops_FiresSuggestionAtThreshold()
    {
        var service = BehaviorLearningService.Instance;
        var spaceId = Guid.NewGuid(); // unique per test run — no cross-test pollution
        string ext = $".test_{spaceId:N}";

        BehaviorLearningService.RuleSuggestion? captured = null;
        service.RuleSuggested += (_, s) =>
        {
            if (s.Extension == ext && s.SpaceId == spaceId)
                captured = s;
        };

        for (int i = 0; i < BehaviorLearningService.SuggestionThreshold; i++)
            service.RecordItemDroppedToSpace(ext, spaceId, "TestSpace");

        captured.Should().NotBeNull("suggestion should fire exactly at the threshold");
        captured!.MoveCount.Should().Be(BehaviorLearningService.SuggestionThreshold);
        captured.SpaceTitle.Should().Be("TestSpace");
    }

    [Fact]
    public void BehaviorLearning_ExtensionNormalised_UpperAndLowerMatch()
    {
        var service = BehaviorLearningService.Instance;
        var spaceId = Guid.NewGuid();
        string extBase = $".mix_{Guid.NewGuid():N}";
        string upperExt = extBase.ToUpperInvariant();
        string lowerExt = extBase.ToLowerInvariant();

        // Record 2 with upper-case → before threshold
        service.RecordItemDroppedToSpace(upperExt, spaceId, "Docs");
        service.RecordItemDroppedToSpace(upperExt, spaceId, "Docs");

        // GetPendingSuggestions should find nothing yet (need 3)
        var pending = service.GetPendingSuggestions();
        pending.Should().NotContain(s => s.SpaceId == spaceId);

        // Third with lower-case — normalisation must make them the same extension
        service.RecordItemDroppedToSpace(lowerExt, spaceId, "Docs");

        pending = service.GetPendingSuggestions();
        pending.Should().Contain(s => s.SpaceId == spaceId && string.Equals(s.Extension, lowerExt, StringComparison.Ordinal),
            "upper and lower-case extensions should be treated as the same key");
    }

    [Fact]
    public void BehaviorLearning_EmptyExtension_Ignored()
    {
        // Must not throw and must not change observable state.
        var before = BehaviorLearningService.Instance.GetPendingSuggestions().Count;

        BehaviorLearningService.Instance.RecordItemDroppedToSpace(string.Empty, Guid.NewGuid(), "Space");
        BehaviorLearningService.Instance.RecordItemDroppedToSpace("   ", Guid.NewGuid(), "Space");

        BehaviorLearningService.Instance.GetPendingSuggestions().Count.Should().Be(before);
    }

    // ─── SpacePortalSyncManager ───────────────────────────────────────────────

    [Fact]
    public void SpacePortalSyncManager_RegisterAndUnregister_TracksWatchState()
    {
        var tempDir = Path.Combine(Path.GetTempPath(), $"ivoe_portal_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        try
        {
            var space = new SpaceModel
            {
                Id    = Guid.NewGuid(),
                Title = "PortalIntegrationTest",
                Type  = SpaceType.Portal,
                PortalFolderPath = tempDir,
            };

            var mgr = SpacePortalSyncManager.Instance;

            mgr.IsPortalSpaceWatched(space.Id).Should().BeFalse("space not yet registered");

            mgr.RegisterPortalSpace(space, tempDir);
            mgr.IsPortalSpaceWatched(space.Id).Should().BeTrue("space should be watched after registration");

            mgr.UnregisterPortalSpace(space.Id);
            mgr.IsPortalSpaceWatched(space.Id).Should().BeFalse("space should no longer be watched after removal");
        }
        finally
        {
            Directory.Delete(tempDir, recursive: true);
        }
    }

    [Fact]
    public void SpacePortalSyncManager_RegisterWithMissingFolder_DoesNotWatch()
    {
        var space = new SpaceModel
        {
            Id    = Guid.NewGuid(),
            Title = "MissingFolderTest",
        };
        string nonExistentPath = Path.Combine(Path.GetTempPath(), $"no_such_dir_{Guid.NewGuid():N}");

        SpacePortalSyncManager.Instance.RegisterPortalSpace(space, nonExistentPath);

        SpacePortalSyncManager.Instance.IsPortalSpaceWatched(space.Id)
            .Should().BeFalse("cannot watch a folder that does not exist");
    }

    // ─── DynamicSpaceScheduler ────────────────────────────────────────────────

    [Fact]
    public void DynamicSpaceScheduler_ApplyTimeWindow_DoesNotThrowWhenNoSpaces()
    {
        // SpaceStateService.Spaces is empty in the test process; the scheduler
        // must handle this gracefully with no exception.
        var act = () => DynamicSpaceScheduler.Instance.ApplyTimeWindowVisibility(DateTime.Now);
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
