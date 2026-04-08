using IVOESpaces.Core;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Native;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Handles built-in command registration and command palette execution flow.
/// </summary>
internal sealed class CommandPaletteCoordinator
{
    private readonly WorkspaceCoordinator _workspace;
    private readonly Func<Task<Guid>> _createSpace;

    public CommandPaletteCoordinator(WorkspaceCoordinator workspace, Func<Task<Guid>> createSpace)
    {
        _workspace = workspace;
        _createSpace = createSpace;
    }

    public void RegisterBuiltInCommands()
    {
        CommandPaletteService.Instance.Register(new CommandPaletteEntry
        {
            Id = "cmd.new-space",
            Title = "Create New Space",
            Subtitle = AppIdentity.ProductName,
            Type = CommandPaletteEntryType.QuickAction,
            Score = 1.0,
            Execute = () => _ = _createSpace(),
        });

        CommandPaletteService.Instance.Register(new CommandPaletteEntry
        {
            Id = "cmd.cleanup-desktop",
            Title = "Clean Up Desktop Layout",
            Subtitle = "Smart non-overlapping arrangement",
            Type = CommandPaletteEntryType.QuickAction,
            Score = 0.95,
            Execute = _workspace.CleanUpDesktopLayout,
        });

        int idx = 0;
        foreach (var profile in SpaceProfileService.Instance.Profiles)
        {
            int capture = idx;
            var profileId = profile.Id;
            var profileName = profile.Name;
            CommandPaletteService.Instance.Register(new CommandPaletteEntry
            {
                Id = $"cmd.switch-workspace.{profileId}",
                Title = $"Switch Workspace: {profileName}",
                Subtitle = "Profiles",
                Type = CommandPaletteEntryType.SwitchProfile,
                Score = 0.90,
                Execute = () => _workspace.SwitchWorkspaceByIndex(capture),
            });
            idx++;
        }
    }

    public void PromptCommandPalette()
    {
        if (!AppSettingsRepository.Instance.Current.EnableCommandPalette)
            return;

        string? query = Win32InputDialog.Show(
            IntPtr.Zero,
            "Command Palette (Ctrl+Shift+P):",
            AppIdentity.CommandPaletteTitle,
            string.Empty);

        if (query is null)
            return;

        var results = CommandPaletteService.Instance.Search(query.Trim(), take: 1);
        if (results.Count == 0)
        {
            Log.Information("CommandPaletteCoordinator: no command found for '{Query}'", query);
            return;
        }

        var selected = results[0];
        try
        {
            selected.Execute?.Invoke();
            Log.Information("CommandPaletteCoordinator: executed '{Title}'", selected.Title);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "CommandPaletteCoordinator: command failed '{Title}'", selected.Title);
        }
    }
}
