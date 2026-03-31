using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class SettingsManager
{
    private static readonly Lazy<SettingsManager> _instance = new(() => new SettingsManager());

    public static SettingsManager Instance => _instance.Value;

    public AppSettings Current => AppSettingsRepository.Instance.Current;

    public void Update(Action<AppSettings> mutate, params string[] changedKeys)
    {
        mutate(AppSettingsRepository.Instance.Current);

        AppSettingsRepository.Instance.SaveNow();
        AppSettingsRepository.Instance.ApplyStartWithWindows();
        FenceStateService.Instance.MarkDirty();

        if (changedKeys.Length == 0)
        {
            SettingsEvents.RaiseReloaded();
            return;
        }

        foreach (string key in changedKeys)
            SettingsEvents.Raise(key, scope: "global");
    }

    public void NotifyFenceChanged(Guid fenceId, params string[] changedKeys)
    {
        FenceStateService.Instance.MarkDirty();

        if (changedKeys.Length == 0)
        {
            SettingsEvents.RaiseReloaded();
            return;
        }

        foreach (string key in changedKeys)
            SettingsEvents.Raise(key, scope: "fence", fenceId: fenceId);
    }

    public void NotifyProfileActivated(string profileId)
    {
        SettingsEvents.Raise("profile.activated", scope: "profile", value: profileId);
    }

    public void NotifyRuntimeRefresh()
    {
        SettingsEvents.RaiseReloaded();
    }
}