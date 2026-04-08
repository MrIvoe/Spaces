using IVOESpaces.Shell.Native;

namespace IVOESpaces.Shell.Settings;

/// <summary>
/// Reusable GDI row-rendering primitives for the settings window.
/// Extracts the repetitive bool-row, choice-row, action-row, section-header,
/// and button drawing so that per-tab renderers (and the host) stay lean.
/// </summary>
internal sealed class SettingsRowRenderer
{
    private readonly int _contentLeft;
    private readonly Func<int, int> _contentRight;
    private readonly int _rowLabelLeft;
    private readonly Func<int, int> _rowValueLeft;
    private readonly Func<int, int> _rowValueRight;
    private readonly int _rowHeight;
    private readonly List<SettingsHitTarget> _hitTargets;
    private readonly Action<Action, string> _applyImmediate;

    public SettingsRowRenderer(
        int contentLeft,
        Func<int, int> contentRight,
        int rowLabelLeft,
        Func<int, int> rowValueLeft,
        Func<int, int> rowValueRight,
        int rowHeight,
        List<SettingsHitTarget> hitTargets,
        Action<Action, string> applyImmediate)
    {
        _contentLeft = contentLeft;
        _contentRight = contentRight;
        _rowLabelLeft = rowLabelLeft;
        _rowValueLeft = rowValueLeft;
        _rowValueRight = rowValueRight;
        _rowHeight = rowHeight;
        _hitTargets = hitTargets;
        _applyImmediate = applyImmediate;
    }

    public void DrawSectionHeader(IntPtr hdc, string title, int y, int clientRight, bool dark)
    {
        var sectionRect = new Win32.RECT
        {
            left = _contentLeft,
            top = y,
            right = _contentRight(clientRight),
            bottom = y + 24,
        };

        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, SettingsColors.ClrTextMuted(dark));
        Win32.DrawText(hdc, title, -1, ref sectionRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

        var line = new Win32.RECT
        {
            left = sectionRect.left,
            top = sectionRect.bottom - 1,
            right = sectionRect.right,
            bottom = sectionRect.bottom,
        };
        IntPtr brush = Win32.CreateSolidBrush(SettingsColors.ClrBorder(dark));
        Win32.FillRect(hdc, ref line, brush);
        Win32.DeleteObject(brush);
    }

    public int DrawBoolRow(IntPtr hdc, int y, int clientRight, string label, bool value, Action onToggle, bool dark)
    {
        var row = new Win32.RECT { left = _contentLeft, top = y, right = _contentRight(clientRight), bottom = y + _rowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(SettingsColors.ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        var sep = new Win32.RECT { left = _contentLeft, top = y + _rowHeight, right = _contentRight(clientRight), bottom = y + _rowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(SettingsColors.ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = _rowLabelLeft, top = y, right = _rowValueLeft(clientRight) - 12, bottom = y + _rowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, SettingsColors.ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        var box = new Win32.RECT { left = _rowValueRight(clientRight) - 42, top = y + 9, right = _rowValueRight(clientRight), bottom = y + 25 };
        uint switchBg = value ? SettingsColors.ClrAccent(dark) : (dark ? Win32.RGB(70, 70, 76) : Win32.RGB(180, 185, 195));
        IntPtr switchBrush = Win32.CreateSolidBrush(switchBg);
        Win32.FillRect(hdc, ref box, switchBrush);
        Win32.DeleteObject(switchBrush);

        int thumbX = value ? (box.right - 14) : (box.left + 2);
        var thumb = new Win32.RECT { left = thumbX, top = box.top + 2, right = thumbX + 12, bottom = box.bottom - 2 };
        IntPtr thumbBrush = Win32.CreateSolidBrush(Win32.RGB(255, 255, 255));
        Win32.FillRect(hdc, ref thumb, thumbBrush);
        Win32.DeleteObject(thumbBrush);

        _hitTargets.Add(new SettingsHitTarget(row, () => _applyImmediate(onToggle, "Saved")));
        return y + _rowHeight + 1;
    }

    public int DrawChoiceRow(IntPtr hdc, int y, int clientRight, string label, string value, Action onCycle, bool dark)
    {
        var row = new Win32.RECT { left = _contentLeft, top = y, right = _contentRight(clientRight), bottom = y + _rowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(SettingsColors.ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        var sep = new Win32.RECT { left = _contentLeft, top = y + _rowHeight, right = _contentRight(clientRight), bottom = y + _rowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(SettingsColors.ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = _rowLabelLeft, top = y, right = _rowValueLeft(clientRight) - 8, bottom = y + _rowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, SettingsColors.ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        var valRect = new Win32.RECT { left = _rowValueLeft(clientRight), top = y, right = _rowValueRight(clientRight), bottom = y + _rowHeight };
        Win32.SetTextColor(hdc, SettingsColors.ClrAccent(dark));
        Win32.DrawText(hdc, value + "  \u203a", -1, ref valRect, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        _hitTargets.Add(new SettingsHitTarget(row, () => _applyImmediate(onCycle, "Saved")));
        return y + _rowHeight + 1;
    }

    public int DrawActionRow(IntPtr hdc, int y, int clientRight, string label, string actionLabel, Action action, bool dark)
    {
        var row = new Win32.RECT { left = _contentLeft, top = y, right = _contentRight(clientRight), bottom = y + _rowHeight };
        IntPtr rowBrush = Win32.CreateSolidBrush(SettingsColors.ClrSurface(dark));
        Win32.FillRect(hdc, ref row, rowBrush);
        Win32.DeleteObject(rowBrush);

        var sep = new Win32.RECT { left = _contentLeft, top = y + _rowHeight, right = _contentRight(clientRight), bottom = y + _rowHeight + 1 };
        IntPtr sepBrush = Win32.CreateSolidBrush(SettingsColors.ClrBorder(dark));
        Win32.FillRect(hdc, ref sep, sepBrush);
        Win32.DeleteObject(sepBrush);

        var lblRect = new Win32.RECT { left = _rowLabelLeft, top = y, right = _rowValueLeft(clientRight) - 8, bottom = y + _rowHeight };
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, SettingsColors.ClrText(dark));
        Win32.DrawText(hdc, label, -1, ref lblRect, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        var actionRect = new Win32.RECT { left = _rowValueLeft(clientRight), top = y, right = _rowValueRight(clientRight), bottom = y + _rowHeight };
        Win32.SetTextColor(hdc, SettingsColors.ClrAccent(dark));
        Win32.DrawText(hdc, actionLabel + "  \u203a", -1, ref actionRect, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

        _hitTargets.Add(new SettingsHitTarget(row, action));
        return y + _rowHeight + 1;
    }
}

/// <summary>Hit target record shared between the renderer and the window host.</summary>
internal sealed record SettingsHitTarget(Win32.RECT Rect, Action Action);

/// <summary>
/// Extracted color helpers used by both the row renderer and the settings window host.
/// </summary>
internal static class SettingsColors
{
    public static uint ClrBg(bool dark)         => dark ? Win32.RGB(28, 28, 30)    : Win32.RGB(244, 246, 249);
    public static uint ClrSurface(bool dark)    => dark ? Win32.RGB(40, 40, 44)    : Win32.RGB(255, 255, 255);
    public static uint ClrSurfaceAlt(bool dark) => dark ? Win32.RGB(50, 50, 56)    : Win32.RGB(245, 247, 250);
    public static uint ClrText(bool dark)       => dark ? Win32.RGB(220, 220, 220) : Win32.RGB(35, 35, 35);
    public static uint ClrTextMuted(bool dark)  => dark ? Win32.RGB(140, 140, 148) : Win32.RGB(90, 90, 110);
    public static uint ClrAccent(bool dark)     => dark ? Win32.RGB(99, 165, 240)  : Win32.RGB(26, 99, 156);
    public static uint ClrBorder(bool dark)     => dark ? Win32.RGB(60, 60, 66)    : Win32.RGB(210, 215, 222);
    public static uint ClrTabText(bool dark, bool selected) =>
        selected ? ClrAccent(dark) : (dark ? Win32.RGB(175, 175, 185) : Win32.RGB(60, 76, 92));
    public static uint ClrTabActive(bool dark)  => dark ? Win32.RGB(40, 40, 44)    : Win32.RGB(255, 255, 255);
    public static uint ClrTabInactive(bool dark)=> dark ? Win32.RGB(30, 30, 34)    : Win32.RGB(221, 228, 236);
}
