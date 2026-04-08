using System.IO;

namespace IVOESpaces.Core;

/// <summary>
/// Application path constants for IVOE Spaces.
/// Uses IVOESpaces folder structure.
/// Legacy OpenSpaces paths are migrated on first run via AppPathMigration.
/// </summary>
public static class AppPaths
{
    private static readonly bool _isPortable =
        File.Exists(Path.Combine(AppContext.BaseDirectory, "portable.flag"));

    /// <summary>Root data directory for settings, spaces, rules, etc.</summary>
    public static string DataRoot => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "data")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "IVOESpaces");

    public static string SpacesConfig   => Path.Combine(DataRoot, "spaces.json");
    public static string SettingsConfig => Path.Combine(DataRoot, "settings.json");
    public static string RulesConfig    => Path.Combine(DataRoot, "rules.json");
    public static string RulesDsl       => Path.Combine(UserRoot, "rules.dsl");
    public static string BehaviorLog    => Path.Combine(DataRoot, "behavior.json");
    public static string DesktopEntitiesConfig => Path.Combine(DataRoot, "desktop-entities.json");
    public static string PluginSettingsConfig => Path.Combine(DataRoot, "plugin-settings.json");
    public static string PluginHostConfig => Path.Combine(DataRoot, "plugin-host.json");
    public static string PluginTrustPolicyConfig => Path.Combine(DataRoot, "plugin-trust-policy.json");
    public static string PluginInstallJournalConfig => Path.Combine(DataRoot, "plugin-install-journal.json");
    public static string WorkspaceBindingsConfig => Path.Combine(DataRoot, "workspace-bindings.json");
    public static string SnapshotsDir   => Path.Combine(DataRoot, "snapshots");
    public static string ThemesDir      => Path.Combine(DataRoot, "themes");
    public static string LogsDir        => Path.Combine(DataRoot, "logs");
    
    /// <summary>User root directory for workspaces and storage.</summary>
    public static string UserRoot       => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "user")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "IVOESpaces");
    
    public static string WorkspaceRoot  => Path.Combine(UserRoot, "workspaces");
    public static string StorageRoot    => Path.Combine(UserRoot, "storage");
    
    /// <summary>Desktop folder for workspace projection and standard backups.</summary>
    public static string WorkspaceDesktopDir => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "user", "desktop")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            "IVOE Spaces Workspace");
    
    /// <summary>Folder next to the executable where plugin DLLs are placed.</summary>
    public static string PluginsDir     => Path.Combine(AppContext.BaseDirectory, "plugins");
    public static string PluginPackagesDir => Path.Combine(DataRoot, "plugin-packages");
    public static string PluginStagingDir => Path.Combine(PluginPackagesDir, "staging");
    public static string PluginInstalledDir => Path.Combine(PluginPackagesDir, "installed");
    public static string PluginCatalogFile => Path.Combine(PluginPackagesDir, "catalog.json");
    public static string Win32ThemeSystemDir => Path.Combine(ThemesDir, "Win32ThemeSystem");
    public static string Win32ThemeSystemManifest => Path.Combine(Win32ThemeSystemDir, "theme.json");
    
    /// <summary>True if running in portable mode (portable.flag exists in app directory).</summary>
    public static bool   IsPortableMode => _isPortable;

    /// <summary>
    /// Ensures the full folder structure exists.  Call once from Program.Main before settings load.
    /// </summary>
    public static void EnsureDirectories()
    {
        Directory.CreateDirectory(DataRoot);
        Directory.CreateDirectory(UserRoot);
        Directory.CreateDirectory(WorkspaceRoot);
        Directory.CreateDirectory(StorageRoot);
        Directory.CreateDirectory(SnapshotsDir);
        Directory.CreateDirectory(ThemesDir);
        Directory.CreateDirectory(Win32ThemeSystemDir);
        Directory.CreateDirectory(LogsDir);
        Directory.CreateDirectory(PluginsDir);
        Directory.CreateDirectory(PluginPackagesDir);
        Directory.CreateDirectory(PluginStagingDir);
        Directory.CreateDirectory(PluginInstalledDir);
    }
}
