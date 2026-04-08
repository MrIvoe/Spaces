using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public sealed class SpaceProfileService
{
    private static readonly Lazy<SpaceProfileService> _instance = new(() => new SpaceProfileService());
    public static SpaceProfileService Instance => _instance.Value;

    private static readonly AsyncSerialQueue _saveQueue = new();
    private readonly List<SpaceProfileModel> _profiles = new();

    private SpaceProfileService()
    {
        // Always ensure the built-in Default profile exists
        _profiles.Add(new SpaceProfileModel
        {
            Id = "default",
            Name = "Default",
            IsBuiltIn = true,
            IsActive = true,
            Trigger = SpaceProfileTrigger.Manual
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

    public IReadOnlyList<SpaceProfileModel> Profiles => _profiles;

    public SpaceProfileModel? GetActiveProfile() => _profiles.FirstOrDefault(p => p.IsActive);

    public SpaceProfileModel Create(string name)
    {
        var profile = new SpaceProfileModel
        {
            Id = Guid.NewGuid().ToString("N"),
            Name = name,
            Trigger = SpaceProfileTrigger.Manual
        };
        _profiles.Add(profile);
        PersistToSettings();
        return profile;
    }

    public bool Activate(string profileId)
    {
        SpaceProfileModel? target = _profiles.FirstOrDefault(p => p.Id == profileId);
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
            SpaceProfileService.Instance._profiles
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
            Serilog.Log.Warning(ex, "SpaceProfileService: failed to persist profile settings");
        }
    }

}
