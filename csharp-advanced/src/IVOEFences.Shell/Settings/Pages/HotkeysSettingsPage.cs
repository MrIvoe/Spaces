namespace IVOEFences.Shell.Settings.Pages;

internal sealed class HotkeysSettingsPage : ISettingsPage
{
    public string Title => "Hotkeys";

    public void Paint(
        IntPtr hwnd,
        IntPtr hdc,
        int clientRight,
        int clientBottom,
        SettingsRowRenderer rows,
        SettingsContext context)
    {
    }

    public void HandleClick(int x, int y)
    {
    }
}
