using IVOEFences.Core.Models;
using IVOEFences.Core.Services;
using Serilog;

namespace IVOEFences.Shell.Profiles;

internal sealed class ProfileSwitcher : IDisposable
{
    private readonly ProfileManager _profiles;
    private Timer? _timer;

    public ProfileSwitcher(ProfileManager profiles)
    {
        _profiles = profiles;
    }

    public void Start()
    {
        _timer = new Timer(_ => Tick(), null, TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(30));
    }

    public void Stop()
    {
        _timer?.Dispose();
        _timer = null;
    }

    private void Tick()
    {
        AppSettings settings = AppSettingsRepository.Instance.Current;
        if (!settings.AutoSwitchProfiles)
            return;

        var active = _profiles.GetActiveProfile();
        if (active == null)
            return;

        // Time-based context switching hook.
        DynamicFenceScheduler.Instance.ApplyTimeWindowVisibility(DateTime.Now);
        Log.Debug("ProfileSwitcher: applied dynamic schedule for profile {Profile}", active.Id);
    }

    public void Dispose() => Stop();
}
