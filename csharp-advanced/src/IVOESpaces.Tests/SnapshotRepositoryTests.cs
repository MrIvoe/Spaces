using FluentAssertions;
using IVOESpaces.Core.Services;
using System.Reflection;
using Xunit;

namespace IVOESpaces.Tests;

/// <summary>
/// Behavioural tests for SnapshotRepository using an isolated instance
/// (created via reflection) that writes to a tear-down temp directory.
/// This approach avoids touching the user's real snapshot directory.
/// </summary>
public class SnapshotRepositoryTests : IDisposable
{
    private readonly string _tempDir = Path.Combine(Path.GetTempPath(), $"ivoe_snap_{Guid.NewGuid():N}");
    private readonly SnapshotRepository _repo;

    public SnapshotRepositoryTests()
    {
        Directory.CreateDirectory(_tempDir);

        // Create a fresh, non-singleton instance and redirect _snapshotDir to the temp folder.
        _repo = (SnapshotRepository)Activator.CreateInstance(typeof(SnapshotRepository), nonPublic: true)!;
        typeof(SnapshotRepository)
            .GetField("_snapshotDir", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(_repo, _tempDir);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempDir))
            Directory.Delete(_tempDir, recursive: true);
    }

    // ─── CreateSnapshot ─────────────────────────────────────────────────────

    [Fact]
    public void CreateSnapshot_AppearsInGetAllSnapshots()
    {
        var snap = _repo.CreateSnapshot("my-snap", "test description");

        var all = _repo.GetAllSnapshots(includeAutoBackups: false);

        all.Should().ContainSingle(s => s.Id == snap.Id);
        all[0].Name.Should().Be("my-snap");
        all[0].Description.Should().Be("test description");
        all[0].IsAutoBackup.Should().BeFalse();
    }

    [Fact]
    public void CreateSnapshot_StoredFileExists()
    {
        var snap = _repo.CreateSnapshot("persist-test");

        string expectedFile = Path.Combine(_tempDir, $"snapshot_{snap.Id:N}.json");
        File.Exists(expectedFile).Should().BeTrue("snapshot should be written to disk immediately");
    }

    // ─── GetAllSnapshots ordering ────────────────────────────────────────────

    [Fact]
    public void GetAllSnapshots_ReturnedNewestFirst()
    {
        _repo.CreateSnapshot("first");
        Thread.Sleep(15); // ensure different CreatedAt timestamps
        _repo.CreateSnapshot("second");

        var all = _repo.GetAllSnapshots(includeAutoBackups: false);

        all.Should().HaveCount(2);
        all[0].Name.Should().Be("second", "newest snapshot should be listed first");
        all[1].Name.Should().Be("first");
    }

    // ─── DeleteSnapshot ─────────────────────────────────────────────────────

    [Fact]
    public void DeleteSnapshot_RemovesFromDiskAndList()
    {
        var snap = _repo.CreateSnapshot("delete-me");

        bool deleted = _repo.DeleteSnapshot(snap.Id);

        deleted.Should().BeTrue();
        _repo.GetAllSnapshots(includeAutoBackups: false).Should().BeEmpty();
        string filePath = Path.Combine(_tempDir, $"snapshot_{snap.Id:N}.json");
        File.Exists(filePath).Should().BeFalse();
    }

    [Fact]
    public void DeleteSnapshot_UnknownId_ReturnsFalse()
    {
        _repo.DeleteSnapshot(Guid.NewGuid()).Should().BeFalse();
    }

    // ─── RenameSnapshot ──────────────────────────────────────────────────────

    [Fact]
    public void RenameSnapshot_UpdatesNameOnDisk()
    {
        var snap = _repo.CreateSnapshot("original-name");

        bool result = _repo.RenameSnapshot(snap.Id, "new-name");

        result.Should().BeTrue();
        var all = _repo.GetAllSnapshots(includeAutoBackups: false);
        all.Should().ContainSingle(s => s.Id == snap.Id && s.Name == "new-name");
    }

    // ─── CreateAutoBackup ────────────────────────────────────────────────────

    [Fact]
    public void CreateAutoBackup_MarkedAutoBackup_HiddenFromDefaultList()
    {
        _repo.CreateAutoBackup();

        _repo.GetAllSnapshots(includeAutoBackups: false).Should().BeEmpty(
            "auto-backups must not appear in the normal user-visible list");
        _repo.GetAllSnapshots(includeAutoBackups: true).Should().HaveCount(1);
    }

    // ─── CleanupOldBackups ────────────────────────────────────────────────────

    [Fact]
    public void CleanupOldBackups_RemovesOldestBeyondKeepCount()
    {
        for (int i = 0; i < 7; i++)
        {
            Thread.Sleep(10); // distinct timestamps
            _repo.CreateAutoBackup();
        }

        int removed = _repo.CleanupOldBackups(keepCount: 3);

        removed.Should().Be(4);
        _repo.GetAllSnapshots(includeAutoBackups: true).Should().HaveCount(3);
    }
}

