namespace IVOESpaces.Shell.Input;

internal sealed class KeyboardHook
{
    public void RegisterDefaults(IntPtr hwnd)
    {
        _ = hwnd;
        // Placeholder: wire WM_HOTKEY registration through Win32.RegisterHotKey.
    }

    public void UnregisterAll()
    {
        // Placeholder: unregister all hotkeys when keyboard hook is implemented.
    }
}
