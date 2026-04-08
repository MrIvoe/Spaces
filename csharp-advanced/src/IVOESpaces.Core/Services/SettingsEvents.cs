namespace IVOESpaces.Core.Services;

public sealed record SettingChangedEvent(
    string Key,
    string Scope,
    Guid? SpaceId = null,
    object? Value = null);

public static class SettingsEvents
{
    public static event Action<SettingChangedEvent>? SettingChanged;
    public static event Action? GlobalSettingsReloaded;

    public static void Raise(string key, string scope, object? value = null, Guid? spaceId = null)
    {
        SettingChanged?.Invoke(new SettingChangedEvent(key, scope, spaceId, value));
    }

    public static void RaiseReloaded()
    {
        GlobalSettingsReloaded?.Invoke();
    }
}