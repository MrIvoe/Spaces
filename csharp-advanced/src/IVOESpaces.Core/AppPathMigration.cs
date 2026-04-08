using System.IO;

namespace IVOESpaces.Core;

/// <summary>
/// Handles backward-compatible migration from OpenSpaces to IVOESpaces paths.
/// Called early in startup before settings/spaces load.
/// Preserves user data by copying from legacy paths with fallback semantics.
/// </summary>
public static class AppPathMigration
{
    public static string LegacyDataRoot =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            AppIdentity.LegacyName);

    public static string LegacyUserRoot =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            AppIdentity.LegacyName);

    public static string LegacyWorkspaceDesktopDir =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            AppIdentity.LegacyName + " Workspace");

    /// <summary>
    /// Migrates user data from legacy OpenSpaces paths to new IVOESpaces paths.
    /// Only copies if source exists and destination does not yet exist.
    /// Call this very early in startup, before AppSettingsRepository or SpaceStateService load.
    /// </summary>
    public static void MigrateIfNeeded()
    {
        // Skip migration if running in portable mode (portable.flag takes precedence)
        if (AppPaths.IsPortableMode)
            return;

        TryCopyDirectory(LegacyDataRoot, AppPaths.DataRoot);
        TryCopyDirectory(LegacyUserRoot, AppPaths.UserRoot);
        TryRenameWorkspaceDesktopDir();
        TryMigrateStartupRegistryKey();
    }

    /// <summary>
    /// Safely copies a directory tree from source to destination if source exists and destination doesn't.
    /// Preserves directory structure and handles all subdirectories recursively.
    /// </summary>
    private static void TryCopyDirectory(string source, string destination)
    {
        // If source doesn't exist or destination already exists, nothing to migrate
        if (!Directory.Exists(source) || Directory.Exists(destination))
            return;

        try
        {
            Directory.CreateDirectory(destination);

            // Copy all subdirectories first
            foreach (string sourceDir in Directory.GetDirectories(source, "*", SearchOption.AllDirectories))
            {
                string relativePath = Path.GetRelativePath(source, sourceDir);
                string targetDir = Path.Combine(destination, relativePath);
                Directory.CreateDirectory(targetDir);
            }

            // Copy all files
            foreach (string sourceFile in Directory.GetFiles(source, "*", SearchOption.AllDirectories))
            {
                string relativePath = Path.GetRelativePath(source, sourceFile);
                string targetFile = Path.Combine(destination, relativePath);
                File.Copy(sourceFile, targetFile, overwrite: false);
            }
        }
        catch (Exception ex)
        {
            // Log but don't crash on migration failure — app will continue with empty state
            System.Diagnostics.Debug.WriteLine($"AppPathMigration: Failed to copy {source} to {destination}: {ex.Message}");
        }
    }

    /// <summary>
    /// Attempts to rename the legacy "OpenSpaces Workspace" desktop folder to "IVOE Spaces Workspace".
    /// If the new name already exists, leaves both in place.
    /// </summary>
    private static void TryRenameWorkspaceDesktopDir()
    {
        if (!Directory.Exists(LegacyWorkspaceDesktopDir))
            return;

        if (Directory.Exists(AppPaths.WorkspaceDesktopDir))
            return; // New folder already exists, don't overwrite

        try
        {
            Directory.Move(LegacyWorkspaceDesktopDir, AppPaths.WorkspaceDesktopDir);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"AppPathMigration: Failed to rename workspace desktop dir: {ex.Message}");
        }
    }

    /// <summary>
    /// Removes the legacy "OpenSpaces" startup registry key if the new "IVOESpaces" key already exists.
    /// Prevents duplicate startup entries after migration.
    /// </summary>
    private static void TryMigrateStartupRegistryKey()
    {
        try
        {
            using var runKey = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run", writable: true);
            if (runKey == null) return;

            var oldValue = runKey.GetValue(AppIdentity.LegacyName);
            var newValue = runKey.GetValue(AppIdentity.StartupRegistryName);

            // Only remove the legacy entry if the new entry already exists
            if (oldValue != null && newValue != null)
            {
                runKey.DeleteValue(AppIdentity.LegacyName, throwOnMissingValue: false);
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"AppPathMigration: Failed to migrate startup registry key: {ex.Message}");
        }
    }
}
