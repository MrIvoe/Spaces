using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public sealed class FenceProfileService
{
    private static readonly Lazy<FenceProfileService> _instance = new(() => new FenceProfileService());
    public static FenceProfileService Instance => _instance.Value;

    private static readonly AsyncSerialQueue _saveQueue = new();
    private readonly List<FenceProfileModel> _profiles = new();

    private FenceProfileService()
    {
        // Always ensure the built-in Default profile exists
        _profiles.Add(new FenceProfileModel
        {
            Id = "default",
            Name = "Default",
            IsBuiltIn = true,
            IsActive = true,
            Trigger = FenceProfileTrigger.Manual
        });

        // Load user-created profiles persisted in settings
        var stored = AppSettingsRepository.Instance.Current.UserProfiles;
        foreach (var p in stored)
        {
            if (!_profiles.Any(existing => existing.Id == p.Id))
                _profiles.Add(p);
        }

        // Restore the active profile from settings
        var activeId = AppSettingsRepository.Instance.Current.ActiveProfileId;
        if (!string.IsNullOrEmpty(activeId) && activeId != "default")
        {
            foreach (var p in _profiles)
                p.IsActive = false;
            var active = _profiles.FirstOrDefault(p => p.Id == activeId);
            if (active != null)
                active.IsActive = true;
            else
                _profiles[0].IsActive = true;
        }
    }

    public IReadOnlyList<FenceProfileModel> Profiles => _profiles;

    public FenceProfileModel? GetActiveProfile() => _profiles.FirstOrDefault(p => p.IsActive);

    public FenceProfileModel Create(string name)
    {
        var profile = new FenceProfileModel
        {
            Id = Guid.NewGuid().ToString("N"),
            Name = name,
            Trigger = FenceProfileTrigger.Manual
        };
        _profiles.Add(profile);
        PersistToSettings();
        return profile;
    }

    public bool Activate(string profileId)
    {
        FenceProfileModel? target = _profiles.FirstOrDefault(p => p.Id == profileId);
        if (target == null)
            return false;

        foreach (var p in _profiles)
            p.IsActive = false;

        target.IsActive = true;
        AppSettingsRepository.Instance.Current.ActiveProfileId = profileId;
        PersistToSettings();
        return true;
    }

    public bool Remove(string profileId)
    {
        var target = _profiles.FirstOrDefault(p => p.Id == profileId);
        if (target == null || target.IsBuiltIn)
            return false;

        bool removed = _profiles.Remove(target);
        if (removed)
        {
            if (!_profiles.Any(p => p.IsActive))
                _profiles[0].IsActive = true;
            PersistToSettings();
        }

        return removed;
    }

    /// <summary>Persist user-created (non-built-in) profiles to settings.</summary>
    private static void PersistToSettings()
    {
        AppSettingsRepository.Instance.Current.UserProfiles =
            FenceProfileService.Instance._profiles
                .Where(p => !p.IsBuiltIn)
                .ToList();

        _ = _saveQueue.Enqueue(SaveProfilesSafelyAsync);
    }

    internal static Task AwaitPendingSaveTaskAsync()
    {
        return _saveQueue.WhenIdleAsync();
    }

    private static async Task SaveProfilesSafelyAsync()
    {
        try
        {
            await AppSettingsRepository.Instance.SaveAsync().ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "FenceProfileService: failed to persist profile settings");
        }
    }

}
