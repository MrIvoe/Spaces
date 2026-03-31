using Serilog;
using IVOEFences.Core;

namespace IVOEFences.Shell;

internal static class Program
{
    [STAThread]
    static async Task Main()
    {
        // Migrate legacy OpenFences paths to IVOEFences BEFORE anything loads
        AppPathMigration.MigrateIfNeeded();

        // Guarantee the full folder structure exists before anything writes
        AppPaths.EnsureDirectories();

        // Logging
        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Debug()
            .WriteTo.File(
                Path.Combine(AppPaths.LogsDir, "Shell-log-.txt"),
                rollingInterval: RollingInterval.Day,
                retainedFileCountLimit: 7)
            .CreateLogger();

        try
        {
            Log.Information("{Product} Shell starting (pure Win32)", AppIdentity.ProductName);

            // Load application settings before anything else
            await IVOEFences.Core.Services.AppSettingsRepository.Instance.LoadAsync();

            var shell = new ShellHost();
            await shell.RunAsync();
        }
        catch (Exception ex)
        {
            Log.Fatal(ex, "Unhandled exception — {Product} terminating", AppIdentity.ProductName);
        }
        finally
        {
            Log.CloseAndFlush();
        }
    }
}
