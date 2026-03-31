using IVOEFences.Shell.Fences;
using IVOEFences.Core.Models;
using IVOEFences.Core.Services;

namespace IVOEFences.Shell;

/// <summary>
/// Applies runtime settings changes to live fences and subsystems
/// without requiring an app restart.
/// Extracted from ShellHost to isolate settings-change concerns.
/// </summary>
internal sealed class RuntimeSettingsApplier
{
    private readonly FenceManager _fences;
    private readonly HotkeyCoordinator _hotkeys;
    private readonly FenceVisibilityController _visibility;
    private readonly IdleModeService _idleModeService;
    private Action _registerHotkeys;

    public RuntimeSettingsApplier(
        FenceManager fences,
        HotkeyCoordinator hotkeys,
        FenceVisibilityController visibility,
        IdleModeService idleModeService,
        Action registerHotkeys)
    {
        _fences = fences;
        _hotkeys = hotkeys;
        _visibility = visibility;
        _idleModeService = idleModeService;
        _registerHotkeys = registerHotkeys;
    }

    public void OnSettingChanged(SettingChangedEvent evt)
    {
        // Fence-scoped setting events are handled by fence windows directly.
        if (evt.Scope.Equals("fence", StringComparison.OrdinalIgnoreCase))
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
        List<FenceWindow> windows = _fences.Windows.Where(w => w.IsAlive).ToList();

        _hotkeys.UnregisterAll();
        _registerHotkeys();

        foreach (FenceWindow window in windows)
            window.SyncDesktopItemVisibility(s.HideDesktopIconsOutsideFences);

        _fences.ApplyIconSize(s.IconSize);
        _idleModeService.UpdateThreshold(s.IdleThresholdSeconds);

        if (!s.KeepFencesOnTopPeekMode)
            _visibility.EndPeekMode();

        PageService.Instance.ReloadFromSettings();
        _visibility.RecomputeVisibility();

        foreach (FenceWindow window in windows)
        {
            window.SetOpacityPercent(s.FenceOpacity);
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
        List<FenceWindow> windows = _fences.Windows.Where(w => w.IsAlive).ToList();

        switch (key)
        {
            case nameof(AppSettings.EnableGlobalHotkeys):
            case nameof(AppSettings.ToggleHotkey):
            case nameof(AppSettings.SearchHotkey):
                _hotkeys.UnregisterAll();
                _registerHotkeys();
                break;

            case nameof(AppSettings.HideDesktopIconsOutsideFences):
                foreach (FenceWindow window in windows)
                    window.SyncDesktopItemVisibility(s.HideDesktopIconsOutsideFences);
                break;

            case nameof(AppSettings.IconSize):
                _fences.ApplyIconSize(s.IconSize);
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

            case nameof(AppSettings.FenceOpacity):
                foreach (FenceWindow window in windows)
                    window.SetOpacityPercent(s.FenceOpacity);
                _visibility.ClearIdleFade();
                break;

            case nameof(AppSettings.BlurBackground):
            case nameof(AppSettings.GlassStrength):
                foreach (FenceWindow window in windows)
                    window.RefreshBlurBackground();
                needsInvalidate = true;
                break;

            case nameof(AppSettings.EnableAnimations):
                needsInvalidate = true;
                break;

            case nameof(AppSettings.KeepFencesOnTopPeekMode):
                if (!s.KeepFencesOnTopPeekMode)
                    _visibility.EndPeekMode();
                break;

            default:
                // Plugin/global compatibility keys still fall back to full apply.
                Apply();
                return;
        }

        if (needsInvalidate)
        {
            foreach (FenceWindow window in windows)
                window.InvalidateContent();
        }
    }
}
