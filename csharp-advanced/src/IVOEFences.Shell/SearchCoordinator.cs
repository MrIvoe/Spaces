using IVOEFences.Core;
using IVOEFences.Core.Services;
using IVOEFences.Shell.Fences;
using IVOEFences.Shell.Native;
using IVOEFences.Shell.Settings;
using Serilog;

namespace IVOEFences.Shell;

/// <summary>
/// Handles global search prompt and applying the search query across all fences.
/// </summary>
internal sealed class SearchCoordinator
{
    private readonly FenceManager _fences;
    private string _globalSearchQuery = string.Empty;

    public SearchCoordinator(FenceManager fences)
    {
        _fences = fences;
    }

    public void PromptSearchAcrossFences()
    {
        var settings = AppSettingsRepository.Instance.Current;
        if (!settings.EnableFenceSearch)
        {
            Log.Information("SearchCoordinator: global search hotkey ignored because search is disabled");
            return;
        }

        string? value = Win32InputDialog.Show(
            IntPtr.Zero,
            "Search all fences (leave empty to clear):",
            AppIdentity.SearchDialogTitle,
            _globalSearchQuery);
        if (value is null)
            return;

        _globalSearchQuery = value.Trim();
        _fences.SetGlobalSearchQuery(_globalSearchQuery);

        Log.Information("SearchCoordinator: applied global search query '{Query}'", _globalSearchQuery);
    }
}
