namespace IVOESpaces.Shell.Settings.Pages;

internal sealed class DesktopPagesSettingsPage : ISettingsPage
{
    public string Title => "Pages";

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
