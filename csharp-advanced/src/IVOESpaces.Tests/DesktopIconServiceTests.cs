using FluentAssertions;
using IVOESpaces.Core;
using IVOESpaces.Core.Services;
using Xunit;

namespace IVOESpaces.Tests;

[Collection("AppSettingsRepository")]
public class AppSettingsRepositoryTests : IAsyncLifetime
{
    private string _tempDir = string.Empty;
    private string _settingsPath = string.Empty;
    private AppSettingsRepository _service = null!;

    public async Task InitializeAsync()
    {
        _tempDir = Path.Combine(Path.GetTempPath(), $"ivoespaces-appsettings-tests-{Guid.NewGuid():N}");
        Directory.CreateDirectory(_tempDir);
        _settingsPath = Path.Combine(_tempDir, "appsettings.json");
        _service = AppSettingsRepository.CreateForTesting(_settingsPath);
        await _service.LoadAsync();
    }

    public async Task DisposeAsync()
    {
        await _service.LoadAsync();
        try
        {
            if (Directory.Exists(_tempDir))
            {
                Directory.Delete(_tempDir, recursive: true);
            }
        }
        catch
        {
            // Ignore cleanup races to keep test teardown resilient.
        }
    }

    [Fact]
    public async Task SaveNow_ThenLoadAsync_PreservesUpdatedValues()
    {
        string toggle = "Ctrl+Alt+Shift+9";
        string search = "Ctrl+Alt+S";

        _service.Current.ToggleHotkey = toggle;
        _service.Current.SearchHotkey = search;
        _service.Current.SpaceOpacity = 73;
        _service.SaveNow();

        await _service.LoadAsync();

        _service.Current.ToggleHotkey.Should().Be(toggle);
        _service.Current.SearchHotkey.Should().Be(search);
        _service.Current.SpaceOpacity.Should().Be(73);
    }

    [Fact]
    public async Task LoadAsync_WhenPrimaryMissing_UsesBackupCandidate()
    {
        string primary = _settingsPath;
        string bak = primary + ".bak";

        Directory.CreateDirectory(Path.GetDirectoryName(primary) ?? ".");
        _service.Current.ToggleHotkey = "Win+J";
        await _service.SaveAsync();

        string json = File.ReadAllText(primary);
        File.WriteAllText(bak, json);
        File.Delete(primary);

        await _service.LoadAsync();

        _service.Current.ToggleHotkey.Should().Be("Win+J");
    }

    [Fact]
    public async Task LoadAsync_MigratesLegacyVersionToCurrent()
    {
        string primary = _settingsPath;
        Directory.CreateDirectory(Path.GetDirectoryName(primary) ?? ".");

        var legacyJson = """
        {
          "SettingsVersion": 0,
          "IdleThresholdSeconds": -1,
          "IdleFadeOpacity": 999,
          "ToggleHotkey": "Win+Space"
        }
        """;
        File.WriteAllText(primary, legacyJson);

        await _service.LoadAsync();

        _service.Current.SettingsVersion.Should().Be(1);
        _service.Current.IdleThresholdSeconds.Should().Be(300);
        _service.Current.IdleFadeOpacity.Should().Be(30);
    }
}
