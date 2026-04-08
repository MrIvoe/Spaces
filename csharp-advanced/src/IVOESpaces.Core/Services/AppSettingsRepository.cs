using System.Text.Json;
using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

/// <summary>
/// Singleton repository for <see cref="AppSettings"/>.
/// Persists to <c>%APPDATA%\IVOESpaces\settings.json</c> (or portable equivalent).
/// </summary>
public sealed class AppSettingsRepository
{
    private const int CurrentSettingsVersion = 1;

    private static readonly Lazy<AppSettingsRepository> _lazy =
        new(() => new AppSettingsRepository());

    public static AppSettingsRepository Instance => _lazy.Value;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
    };

    private readonly string _settingsFilePath;
    private string FilePath => _settingsFilePath;
    private string Backup1Path => FilePath + ".bak";
    private string Backup2Path => FilePath + ".bak2";
    private string Backup3Path => FilePath + ".bak3";

    public AppSettings Current { get; private set; } = new();

    private AppSettingsRepository() : this(AppPaths.SettingsConfig)
    {
    }

    internal AppSettingsRepository(string settingsFilePath)
    {
        _settingsFilePath = settingsFilePath;
    }

    public static AppSettingsRepository CreateForTesting(string settingsFilePath)
    {
        return new AppSettingsRepository(settingsFilePath);
    }

    public async Task LoadAsync()
    {
        try
        {
            string[] candidates = { FilePath, Backup1Path, Backup2Path, Backup3Path };
            foreach (string candidate in candidates)
            {
                if (!File.Exists(candidate))
                    continue;

                try
                {
                    await using var stream = File.OpenRead(candidate);
                    AppSettings? loaded = await JsonSerializer.DeserializeAsync<AppSettings>(stream, JsonOptions);
                    if (loaded is null)
                        continue;

                    Current = loaded;
                    MigrateIfNeeded(Current);
                    Serilog.Log.Information("AppSettingsRepository: loaded settings from {Path}", candidate);
                    return;
                }
                catch (Exception ex)
                {
                    Serilog.Log.Warning(ex, "AppSettingsRepository: failed to load candidate {Path}", candidate);
                }
            }

            Current = new AppSettings();
            MigrateIfNeeded(Current);
            Serilog.Log.Warning("AppSettingsRepository: no valid settings file found — using defaults");
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "AppSettingsRepository: failed to load — using defaults");
            Current = new AppSettings();
            MigrateIfNeeded(Current);
        }
    }

    private static void MigrateIfNeeded(AppSettings settings)
    {
        if (settings.SettingsVersion >= CurrentSettingsVersion)
            return;

        // v1: introduce idle fade settings and sane bounds.
        if (settings.SettingsVersion < 1)
        {
            if (settings.IdleThresholdSeconds < 0)
                settings.IdleThresholdSeconds = 300;

            if (settings.IdleFadeOpacity < 0 || settings.IdleFadeOpacity > 100)
                settings.IdleFadeOpacity = 30;
        }

        settings.SettingsVersion = CurrentSettingsVersion;
        Serilog.Log.Information("AppSettingsRepository: migrated settings to version {Version}", CurrentSettingsVersion);
    }

    public async Task SaveAsync()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);

            string tempPath = Path.GetTempFileName();
            await using (var stream = File.Create(tempPath))
            {
                await JsonSerializer.SerializeAsync(stream, Current, JsonOptions);
            }

            RotateSettingsBackups();

            if (File.Exists(FilePath))
                File.Replace(tempPath, FilePath, Backup1Path);
            else
                File.Move(tempPath, FilePath);

            Serilog.Log.Information("AppSettingsRepository: saved settings to {Path}", FilePath);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "AppSettingsRepository: failed to save settings");
        }
    }

    public void SaveNow()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);

            string tempPath = Path.GetTempFileName();
            using (var stream = File.Create(tempPath))
            {
                JsonSerializer.Serialize(stream, Current, JsonOptions);
            }

            RotateSettingsBackups();

            if (File.Exists(FilePath))
                File.Replace(tempPath, FilePath, Backup1Path);
            else
                File.Move(tempPath, FilePath);

            Serilog.Log.Information("AppSettingsRepository: saved settings to {Path}", FilePath);
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "AppSettingsRepository: failed to save settings");
        }
    }

    private void RotateSettingsBackups()
    {
        try
        {
            if (File.Exists(Backup2Path))
                File.Copy(Backup2Path, Backup3Path, overwrite: true);

            if (File.Exists(Backup1Path))
                File.Copy(Backup1Path, Backup2Path, overwrite: true);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "AppSettingsRepository: backup rotation failed");
        }
    }

    /// <summary>
    /// Applies the <see cref="AppSettings.StartWithWindows"/> setting to the
    /// Windows startup registry key (HKCU\...\Run).
    /// Safe to call from any thread.
    /// </summary>
    public void ApplyStartWithWindows()
    {
        try
        {
            const string RegPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
            const string AppName = AppIdentity.StartupRegistryName;

            using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(RegPath, writable: true);
            if (key == null) return;

            if (Current.StartWithWindows)
            {
                string exePath = Environment.ProcessPath
                                 ?? System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName
                                 ?? Path.Combine(AppContext.BaseDirectory, AppIdentity.InternalName + ".exe");
                key.SetValue(AppName, $"\"{exePath}\"");
                Serilog.Log.Information("AppSettingsRepository: registered startup entry");
            }
            else
            {
                key.DeleteValue(AppName, throwOnMissingValue: false);
                Serilog.Log.Information("AppSettingsRepository: removed startup entry");
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "AppSettingsRepository: could not write startup registry key");
        }
    }
}
