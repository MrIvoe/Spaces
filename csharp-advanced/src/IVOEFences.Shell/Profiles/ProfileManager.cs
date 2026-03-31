using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.Profiles;

internal sealed class ProfileManager
{
    public event EventHandler<string>? ProfileActivated;

    public IReadOnlyList<FenceProfileModel> GetProfiles() => FenceProfileService.Instance.Profiles;

    public FenceProfileModel? GetActiveProfile() => FenceProfileService.Instance.GetActiveProfile();

    public FenceProfileModel CreateProfile(string name)
    {
        return FenceProfileService.Instance.Create(name);
    }

    public bool Activate(string profileId)
    {
        bool ok = FenceProfileService.Instance.Activate(profileId);
        if (!ok)
            return false;

        FenceProfileModel? active = FenceProfileService.Instance.GetActiveProfile();
        if (active != null)
            DynamicFenceScheduler.Instance.ApplyProfileVisibility(active);

        SettingsManager.Instance.NotifyProfileActivated(profileId);
        ProfileActivated?.Invoke(this, profileId);
        Log.Information("ProfileManager: activated profile {ProfileId}", profileId);
        return true;
    }
}
