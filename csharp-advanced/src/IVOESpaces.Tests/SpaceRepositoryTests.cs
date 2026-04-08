using FluentAssertions;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

/// <summary>
/// Behavioural tests for SpaceRepository: covers the save/load round-trip,
/// upsert, delete, and candidate fallback (backup file) logic.
///
/// Tests run against isolated temp directories so they are deterministic,
/// parallel-safe, and independent from user app-data.
/// </summary>
public class SpaceRepositoryTests : IDisposable
{
    private readonly string _tempRoot;
    private readonly string _spacesPath;
    private readonly SpaceRepository _repository;

    public SpaceRepositoryTests()
    {
        _tempRoot = Path.Combine(Path.GetTempPath(), $"ivoe_repo_tests_{Guid.NewGuid():N}");
        Directory.CreateDirectory(_tempRoot);
        _spacesPath = Path.Combine(_tempRoot, "spaces.json");
        _repository = SpaceRepository.CreateForTesting(_spacesPath);
    }

    public void Dispose()
    {
        try
        {
            if (Directory.Exists(_tempRoot))
                Directory.Delete(_tempRoot, recursive: true);
        }
        catch
        {
            // Best-effort cleanup for test temp artifacts.
        }
    }

    // ─── Singleton identity ─────────────────────────────────────────────────

    [Fact]
    public void Instance_IsSameObject_OnRepeatedAccess()
    {
        SpaceRepository.Instance.Should().BeSameAs(SpaceRepository.Instance);
    }

    [Fact]
    public void CreateForTesting_UsesDistinctRepositoryInstances()
    {
        var second = SpaceRepository.CreateForTesting(Path.Combine(_tempRoot, "other-spaces.json"));

        second.Should().NotBeSameAs(_repository);
    }

    // ─── Round-trip save / load ────────────────────────────────────────────

    [Fact]
    public async Task SaveAllAsync_ThenLoadAllAsync_ReturnsIdenticalSpaces()
    {
        var space = new SpaceModel
        {
            Id       = Guid.NewGuid(),
            Title    = "RoundTripSpace",
            XFraction = 0.1,
            YFraction = 0.2,
            WidthFraction = 0.3,
            HeightFraction = 0.4,
        };

        await _repository.SaveAllAsync(new[] { space });
        var loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == space.Id
            && f.Title == space.Title
            && f.XFraction == space.XFraction
            && f.WidthFraction == space.WidthFraction);
    }

    // ─── SaveOneAsync upsert behaviour ────────────────────────────────────

    [Fact]
    public async Task SaveOneAsync_WithNewSpace_AddsToList()
    {
        await _repository.SaveAllAsync(Array.Empty<SpaceModel>()); // start from clean state
        var newSpace = new SpaceModel { Id = Guid.NewGuid(), Title = "NewSpace" };

        await _repository.SaveOneAsync(newSpace);
        var loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == newSpace.Id);
    }

    [Fact]
    public async Task SaveOneAsync_WithExistingId_UpdatesTitle()
    {
        var space = new SpaceModel { Id = Guid.NewGuid(), Title = "OriginalTitle" };
        await _repository.SaveAllAsync(new[] { space });

        var updated = space with { Title = "UpdatedTitle" };
        await _repository.SaveOneAsync(updated);

        var loaded = await _repository.LoadAllAsync();
        loaded.Should().ContainSingle(f => f.Id == space.Id && f.Title == "UpdatedTitle");
    }

    // ─── DeleteAsync behaviour ─────────────────────────────────────────────

    [Fact]
    public async Task DeleteAsync_RemovesSpaceFromPersistence()
    {
        var keep   = new SpaceModel { Id = Guid.NewGuid(), Title = "KeepMe" };
        var remove = new SpaceModel { Id = Guid.NewGuid(), Title = "RemoveMe" };
        await _repository.SaveAllAsync(new[] { keep, remove });

        await _repository.DeleteAsync(remove.Id);

        var loaded = await _repository.LoadAllAsync();
        loaded.Should().ContainSingle(f => f.Id == keep.Id);
        loaded.Should().NotContain(f => f.Id == remove.Id);
    }

    // ─── Backup / candidate fallback ─────────────────────────────────────

    [Fact]
    public async Task LoadAllAsync_WhenPrimaryMissing_FallsBackToBackup()
    {
        var space  = new SpaceModel { Id = Guid.NewGuid(), Title = "BackupSpace" };
        string primary = _spacesPath;
        string backup  = primary + ".bak";

        // Directly write space JSON to the backup file so we fully control its content
        // (avoids relying on backup-rotation order which depends on prior file state).
        string spaceJson = System.Text.Json.JsonSerializer.Serialize(
            new[] { space },
            new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented               = true,
                PropertyNameCaseInsensitive = true,
            });

        Directory.CreateDirectory(Path.GetDirectoryName(primary) ?? ".");
        File.WriteAllText(backup, spaceJson);
        if (File.Exists(primary)) File.Delete(primary);

        var loaded = await _repository.LoadAllAsync();
        loaded.Should().Contain(f => f.Id == space.Id,
            "the backup file should act as primary when the primary is missing");
    }

    [Fact]
    public async Task LoadAllAsync_WhenPrimaryCorrupt_FallsBackToBackupCandidate()
    {
        var space = new SpaceModel { Id = Guid.NewGuid(), Title = "BackupOnCorruptPrimary" };
        string primary = _spacesPath;
        string backup = _spacesPath + ".bak";

        File.WriteAllText(primary, "{ this is not valid json }");
        string backupJson = System.Text.Json.JsonSerializer.Serialize(new[] { space });
        File.WriteAllText(backup, backupJson);

        List<SpaceModel> loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == space.Id && f.Title == space.Title);
    }

    [Fact]
    public async Task LoadAllAsync_WithPartialJson_DoesNotThrowAndProducesDefaults()
    {
        string primary = _spacesPath;
        File.WriteAllText(primary, "[{\"Title\":\"Partial\"}]");

        Func<Task<List<SpaceModel>>> act = () => _repository.LoadAllAsync();
        await act.Should().NotThrowAsync();

        List<SpaceModel> loaded = await _repository.LoadAllAsync();
        loaded.Should().ContainSingle();
        loaded[0].Title.Should().Be("Partial");
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotMissing_ReturnsEmpty()
    {
        List<SpaceModel> loaded = await _repository.LoadLayoutSnapshotAsync("missing-hash");

        loaded.Should().BeEmpty();
    }

    [Fact]
    public async Task SaveLayoutSnapshotAsync_ThenLoadLayoutSnapshotAsync_RoundTripsSpaces()
    {
        var space = new SpaceModel
        {
            Id = Guid.NewGuid(),
            Title = "LayoutRoundTrip",
            WidthFraction = 0.2,
            HeightFraction = 0.3,
        };

        await _repository.SaveLayoutSnapshotAsync("layout-hash", new List<SpaceModel> { space });
        List<SpaceModel> loaded = await _repository.LoadLayoutSnapshotAsync("layout-hash");

        loaded.Should().ContainSingle(f => f.Id == space.Id && f.Title == space.Title);
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotCorrupt_ReturnsEmpty()
    {
        string layoutPath = GetLayoutSnapshotPath("corrupt-layout");
        Directory.CreateDirectory(Path.GetDirectoryName(layoutPath)!);
        File.WriteAllText(layoutPath, "{ this is not valid json }");

        List<SpaceModel> loaded = await _repository.LoadLayoutSnapshotAsync("corrupt-layout");

        loaded.Should().BeEmpty();
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotMissingSpacesNode_ReturnsEmpty()
    {
        string layoutPath = GetLayoutSnapshotPath("stale-layout");
        Directory.CreateDirectory(Path.GetDirectoryName(layoutPath)!);
        File.WriteAllText(layoutPath, "{\"ConfigHash\":\"stale-layout\",\"SavedAt\":\"2026-01-01T00:00:00Z\"}");

        List<SpaceModel> loaded = await _repository.LoadLayoutSnapshotAsync("stale-layout");

        loaded.Should().BeEmpty();
    }

    private string GetLayoutSnapshotPath(string configHash)
    {
        return Path.Combine(Path.GetDirectoryName(_spacesPath) ?? ".", "layouts", $"config_{configHash}.json");
    }
}

