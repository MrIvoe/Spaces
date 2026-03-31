using FluentAssertions;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Xunit;

namespace IVOEFences.Tests;

/// <summary>
/// Behavioural tests for FenceRepository: covers the save/load round-trip,
/// upsert, delete, and candidate fallback (backup file) logic.
///
/// Tests run against isolated temp directories so they are deterministic,
/// parallel-safe, and independent from user app-data.
/// </summary>
public class FenceRepositoryTests : IDisposable
{
    private readonly string _tempRoot;
    private readonly string _fencesPath;
    private readonly FenceRepository _repository;

    public FenceRepositoryTests()
    {
        _tempRoot = Path.Combine(Path.GetTempPath(), $"ivoe_repo_tests_{Guid.NewGuid():N}");
        Directory.CreateDirectory(_tempRoot);
        _fencesPath = Path.Combine(_tempRoot, "fences.json");
        _repository = FenceRepository.CreateForTesting(_fencesPath);
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
        FenceRepository.Instance.Should().BeSameAs(FenceRepository.Instance);
    }

    [Fact]
    public void CreateForTesting_UsesDistinctRepositoryInstances()
    {
        var second = FenceRepository.CreateForTesting(Path.Combine(_tempRoot, "other-fences.json"));

        second.Should().NotBeSameAs(_repository);
    }

    // ─── Round-trip save / load ────────────────────────────────────────────

    [Fact]
    public async Task SaveAllAsync_ThenLoadAllAsync_ReturnsIdenticalFences()
    {
        var fence = new FenceModel
        {
            Id       = Guid.NewGuid(),
            Title    = "RoundTripFence",
            XFraction = 0.1,
            YFraction = 0.2,
            WidthFraction = 0.3,
            HeightFraction = 0.4,
        };

        await _repository.SaveAllAsync(new[] { fence });
        var loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == fence.Id
            && f.Title == fence.Title
            && f.XFraction == fence.XFraction
            && f.WidthFraction == fence.WidthFraction);
    }

    // ─── SaveOneAsync upsert behaviour ────────────────────────────────────

    [Fact]
    public async Task SaveOneAsync_WithNewFence_AddsToList()
    {
        await _repository.SaveAllAsync(Array.Empty<FenceModel>()); // start from clean state
        var newFence = new FenceModel { Id = Guid.NewGuid(), Title = "NewFence" };

        await _repository.SaveOneAsync(newFence);
        var loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == newFence.Id);
    }

    [Fact]
    public async Task SaveOneAsync_WithExistingId_UpdatesTitle()
    {
        var fence = new FenceModel { Id = Guid.NewGuid(), Title = "OriginalTitle" };
        await _repository.SaveAllAsync(new[] { fence });

        var updated = fence with { Title = "UpdatedTitle" };
        await _repository.SaveOneAsync(updated);

        var loaded = await _repository.LoadAllAsync();
        loaded.Should().ContainSingle(f => f.Id == fence.Id && f.Title == "UpdatedTitle");
    }

    // ─── DeleteAsync behaviour ─────────────────────────────────────────────

    [Fact]
    public async Task DeleteAsync_RemovesFenceFromPersistence()
    {
        var keep   = new FenceModel { Id = Guid.NewGuid(), Title = "KeepMe" };
        var remove = new FenceModel { Id = Guid.NewGuid(), Title = "RemoveMe" };
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
        var fence  = new FenceModel { Id = Guid.NewGuid(), Title = "BackupFence" };
        string primary = _fencesPath;
        string backup  = primary + ".bak";

        // Directly write fence JSON to the backup file so we fully control its content
        // (avoids relying on backup-rotation order which depends on prior file state).
        string fenceJson = System.Text.Json.JsonSerializer.Serialize(
            new[] { fence },
            new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented               = true,
                PropertyNameCaseInsensitive = true,
            });

        Directory.CreateDirectory(Path.GetDirectoryName(primary) ?? ".");
        File.WriteAllText(backup, fenceJson);
        if (File.Exists(primary)) File.Delete(primary);

        var loaded = await _repository.LoadAllAsync();
        loaded.Should().Contain(f => f.Id == fence.Id,
            "the backup file should act as primary when the primary is missing");
    }

    [Fact]
    public async Task LoadAllAsync_WhenPrimaryCorrupt_FallsBackToBackupCandidate()
    {
        var fence = new FenceModel { Id = Guid.NewGuid(), Title = "BackupOnCorruptPrimary" };
        string primary = _fencesPath;
        string backup = _fencesPath + ".bak";

        File.WriteAllText(primary, "{ this is not valid json }");
        string backupJson = System.Text.Json.JsonSerializer.Serialize(new[] { fence });
        File.WriteAllText(backup, backupJson);

        List<FenceModel> loaded = await _repository.LoadAllAsync();

        loaded.Should().ContainSingle(f => f.Id == fence.Id && f.Title == fence.Title);
    }

    [Fact]
    public async Task LoadAllAsync_WithPartialJson_DoesNotThrowAndProducesDefaults()
    {
        string primary = _fencesPath;
        File.WriteAllText(primary, "[{\"Title\":\"Partial\"}]");

        Func<Task<List<FenceModel>>> act = () => _repository.LoadAllAsync();
        await act.Should().NotThrowAsync();

        List<FenceModel> loaded = await _repository.LoadAllAsync();
        loaded.Should().ContainSingle();
        loaded[0].Title.Should().Be("Partial");
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotMissing_ReturnsEmpty()
    {
        List<FenceModel> loaded = await _repository.LoadLayoutSnapshotAsync("missing-hash");

        loaded.Should().BeEmpty();
    }

    [Fact]
    public async Task SaveLayoutSnapshotAsync_ThenLoadLayoutSnapshotAsync_RoundTripsFences()
    {
        var fence = new FenceModel
        {
            Id = Guid.NewGuid(),
            Title = "LayoutRoundTrip",
            WidthFraction = 0.2,
            HeightFraction = 0.3,
        };

        await _repository.SaveLayoutSnapshotAsync("layout-hash", new List<FenceModel> { fence });
        List<FenceModel> loaded = await _repository.LoadLayoutSnapshotAsync("layout-hash");

        loaded.Should().ContainSingle(f => f.Id == fence.Id && f.Title == fence.Title);
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotCorrupt_ReturnsEmpty()
    {
        string layoutPath = GetLayoutSnapshotPath("corrupt-layout");
        Directory.CreateDirectory(Path.GetDirectoryName(layoutPath)!);
        File.WriteAllText(layoutPath, "{ this is not valid json }");

        List<FenceModel> loaded = await _repository.LoadLayoutSnapshotAsync("corrupt-layout");

        loaded.Should().BeEmpty();
    }

    [Fact]
    public async Task LoadLayoutSnapshotAsync_WhenSnapshotMissingFencesNode_ReturnsEmpty()
    {
        string layoutPath = GetLayoutSnapshotPath("stale-layout");
        Directory.CreateDirectory(Path.GetDirectoryName(layoutPath)!);
        File.WriteAllText(layoutPath, "{\"ConfigHash\":\"stale-layout\",\"SavedAt\":\"2026-01-01T00:00:00Z\"}");

        List<FenceModel> loaded = await _repository.LoadLayoutSnapshotAsync("stale-layout");

        loaded.Should().BeEmpty();
    }

    private string GetLayoutSnapshotPath(string configHash)
    {
        return Path.Combine(Path.GetDirectoryName(_fencesPath) ?? ".", "layouts", $"config_{configHash}.json");
    }
}

