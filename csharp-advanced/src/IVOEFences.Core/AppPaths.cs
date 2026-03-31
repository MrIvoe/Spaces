using System.IO;

namespace IVOEFences.Core;

/// <summary>
/// Application path constants for IVOE Fences.
/// Uses IVOEFences folder structure.
/// Legacy OpenFences paths are migrated on first run via AppPathMigration.
/// </summary>
public static class AppPaths
{
    private static readonly bool _isPortable =
        File.Exists(Path.Combine(AppContext.BaseDirectory, "portable.flag"));

    /// <summary>Root data directory for settings, fences, rules, etc.</summary>
    public static string DataRoot => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "data")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "IVOEFences");

    public static string FencesConfig   => Path.Combine(DataRoot, "fences.json");
    public static string SettingsConfig => Path.Combine(DataRoot, "settings.json");
    public static string RulesConfig    => Path.Combine(DataRoot, "rules.json");
    public static string RulesDsl       => Path.Combine(UserRoot, "rules.dsl");
    public static string BehaviorLog    => Path.Combine(DataRoot, "behavior.json");
    public static string DesktopEntitiesConfig => Path.Combine(DataRoot, "desktop-entities.json");
    public static string PluginSettingsConfig => Path.Combine(DataRoot, "plugin-settings.json");
    public static string WorkspaceBindingsConfig => Path.Combine(DataRoot, "workspace-bindings.json");
    public static string SnapshotsDir   => Path.Combine(DataRoot, "snapshots");
    public static string ThemesDir      => Path.Combine(DataRoot, "themes");
    public static string LogsDir        => Path.Combine(DataRoot, "logs");
    
    /// <summary>User root directory for workspaces and storage.</summary>
    public static string UserRoot       => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "user")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "IVOEFences");
    
    public static string WorkspaceRoot  => Path.Combine(UserRoot, "workspaces");
    public static string StorageRoot    => Path.Combine(UserRoot, "storage");
    
    /// <summary>Desktop folder for workspace projection and standard backups.</summary>
    public static string WorkspaceDesktopDir => _isPortable
        ? Path.Combine(AppContext.BaseDirectory, "user", "desktop")
        : Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            "IVOE Fences Workspace");
    
    /// <summary>Folder next to the executable where plugin DLLs are placed.</summary>
    public static string PluginsDir     => Path.Combine(AppContext.BaseDirectory, "plugins");
    
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
        Directory.CreateDirectory(LogsDir);
        Directory.CreateDirectory(PluginsDir);
    }
}
