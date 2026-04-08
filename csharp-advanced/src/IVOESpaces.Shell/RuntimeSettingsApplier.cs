using IVOESpaces.Shell.Spaces;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;

namespace IVOESpaces.Shell;

/// <summary>
/// Applies runtime settings changes to live spaces and subsystems
/// without requiring an app restart.
/// Extracted from ShellHost to isolate settings-change concerns.
/// </summary>
internal sealed class RuntimeSettingsApplier
{
    private readonly SpaceManager _spaces;
    private readonly HotkeyCoordinator _hotkeys;
    private readonly SpaceVisibilityController _visibility;
    private readonly IdleModeService _idleModeService;
    private Action _registerHotkeys;

    public RuntimeSettingsApplier(
        SpaceManager spaces,
        HotkeyCoordinator hotkeys,
        SpaceVisibilityController visibility,
        IdleModeService idleModeService,
        Action registerHotkeys)
    {
        _spaces = spaces;
        _hotkeys = hotkeys;
        _visibility = visibility;
        _idleModeService = idleModeService;
        _registerHotkeys = registerHotkeys;
    }

    public void OnSettingChanged(SettingChangedEvent evt)
    {
        // Space-scoped setting events are handled by space windows directly.
        if (evt.Scope.Equals("space", StringComparison.OrdinalIgnoreCase))
            return;

        ApplyForKey(evt.Key);
    }

    public void OnGlobalSettingsReloaded()
    {
        Apply();
    }

    public void Apply()
    {
        var s = AppSettingsRepository.Instance.Current;
        List<SpaceWindow> windows = _spaces.Windows.Where(w => w.IsAlive).ToList();

        _hotkeys.UnregisterAll();
        _registerHotkeys();

        foreach (SpaceWindow window in windows)
            window.SyncDesktopItemVisibility(s.HideDesktopIconsOutsideSpaces);

        _spaces.ApplyIconSize(s.IconSize);
        _idleModeService.UpdateThreshold(s.IdleThresholdSeconds);

        if (!s.KeepSpacesOnTopPeekMode)
            _visibility.EndPeekMode();

        PageService.Instance.ReloadFromSettings();
        _visibility.RecomputeVisibility();

        foreach (SpaceWindow window in windows)
        {
            window.SetOpacityPercent(s.SpaceOpacity);
            window.RefreshBlurBackground();
            window.InvalidateContent();
        }

        _visibility.ClearIdleFade();
    }

    private void ApplyForKey(string key)
    {
        if (string.IsNullOrWhiteSpace(key))
        {
            Apply();
            return;
        }

        var s = AppSettingsRepository.Instance.Current;
        bool needsInvalidate = false;
        List<SpaceWindow> windows = _spaces.Windows.Where(w => w.IsAlive).ToList();

        switch (key)
        {
            case nameof(AppSettings.EnableGlobalHotkeys):
            case nameof(AppSettings.ToggleHotkey):
            case nameof(AppSettings.SearchHotkey):
                _hotkeys.UnregisterAll();
                _registerHotkeys();
                break;

            case nameof(AppSettings.HideDesktopIconsOutsideSpaces):
                foreach (SpaceWindow window in windows)
                    window.SyncDesktopItemVisibility(s.HideDesktopIconsOutsideSpaces);
                break;

            case nameof(AppSettings.IconSize):
                _spaces.ApplyIconSize(s.IconSize);
                break;

            case nameof(AppSettings.IdleThresholdSeconds):
                _idleModeService.UpdateThreshold(s.IdleThresholdSeconds);
                break;

            case nameof(AppSettings.EnableDesktopPages):
            case nameof(AppSettings.CurrentDesktopPage):
            case nameof(AppSettings.DesktopPageCount):
                PageService.Instance.ReloadFromSettings();
                _visibility.RecomputeVisibility();
                break;

            case nameof(AppSettings.SpaceOpacity):
                foreach (SpaceWindow window in windows)
                    window.SetOpacityPercent(s.SpaceOpacity);
                _visibility.ClearIdleFade();
                break;

            case nameof(AppSettings.BlurBackground):
            case nameof(AppSettings.GlassStrength):
                foreach (SpaceWindow window in windows)
                    window.RefreshBlurBackground();
                needsInvalidate = true;
                break;

            case nameof(AppSettings.EnableAnimations):
                needsInvalidate = true;
                break;

            case nameof(AppSettings.KeepSpacesOnTopPeekMode):
                if (!s.KeepSpacesOnTopPeekMode)
                    _visibility.EndPeekMode();
                break;

            default:
                // Plugin/global compatibility keys still fall back to full apply.
                Apply();
                return;
        }

        if (needsInvalidate)
        {
            foreach (SpaceWindow window in windows)
                window.InvalidateContent();
        }
    }
}
