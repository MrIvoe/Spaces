using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using Serilog;

namespace IVOESpaces.Shell.Profiles;

internal sealed class ProfileManager
{
    public event EventHandler<string>? ProfileActivated;

    public IReadOnlyList<SpaceProfileModel> GetProfiles() => SpaceProfileService.Instance.Profiles;

    public SpaceProfileModel? GetActiveProfile() => SpaceProfileService.Instance.GetActiveProfile();

    public SpaceProfileModel CreateProfile(string name)
    {
        return SpaceProfileService.Instance.Create(name);
    }

    public bool Activate(string profileId)
    {
        bool ok = SpaceProfileService.Instance.Activate(profileId);
        if (!ok)
            return false;

        SpaceProfileModel? active = SpaceProfileService.Instance.GetActiveProfile();
        if (active != null)
            DynamicSpaceScheduler.Instance.ApplyProfileVisibility(active);

        SettingsManager.Instance.NotifyProfileActivated(profileId);
        ProfileActivated?.Invoke(this, profileId);
        Log.Information("ProfileManager: activated profile {ProfileId}", profileId);
        return true;
    }
}
