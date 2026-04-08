using IVOESpaces.Core;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Spaces;
using IVOESpaces.Shell.Native;
using IVOESpaces.Shell.Settings;
using Serilog;

namespace IVOESpaces.Shell;

/// <summary>
/// Handles global search prompt and applying the search query across all spaces.
/// </summary>
internal sealed class SearchCoordinator
{
    private readonly SpaceManager _spaces;
    private string _globalSearchQuery = string.Empty;

    public SearchCoordinator(SpaceManager spaces)
    {
        _spaces = spaces;
    }

    public void PromptSearchAcrossSpaces()
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableSpaceSearch)
        {
            Log.Information("SearchCoordinator: global search hotkey ignored because search is disabled");
            return;
        }

        string? value = Win32InputDialog.Show(
            IntPtr.Zero,
            "Search all spaces (leave empty to clear):",
            AppIdentity.SearchDialogTitle,
            _globalSearchQuery);
        if (value is null)
            return;

        _globalSearchQuery = value.Trim();
        _spaces.SetGlobalSearchQuery(_globalSearchQuery);

        Log.Information("SearchCoordinator: applied global search query '{Query}'", _globalSearchQuery);
    }
}
