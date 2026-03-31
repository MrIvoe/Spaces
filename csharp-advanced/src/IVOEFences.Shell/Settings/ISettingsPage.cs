namespace IVOEFences.Shell.Settings;

internal interface ISettingsPage
{
    string Title { get; }

    void Paint(
        IntPtr hwnd,
        IntPtr hdc,
        int clientRight,
        int clientBottom,
        SettingsRowRenderer rows,
        SettingsContext context);

    void HandleClick(int x, int y);
}
