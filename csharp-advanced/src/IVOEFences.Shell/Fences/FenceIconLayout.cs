using IVOEFences.Core.Models;
using IVOEFences.Shell.Native;

namespace IVOEFences.Shell.Fences;

/// <summary>
/// Pre-computes the pixel position of every icon in the fence grid and caches the
/// result until the fence is resized.  Keeping the layout out of WM_PAINT means
/// the hit-test and renderer never re-calculate positions on every frame.
///
/// Call <see cref="Invalidate"/> from WM_SIZE; call <see cref="GetPositions"/> from
/// WM_PAINT — positions are recomputed lazily on the first call after invalidation.
/// </summary>
internal sealed class FenceIconLayout
{
    // Layout parameters — kept in sync with FenceRenderer's padding constants
    private const int PadX   = 8;
    private const int PadY   = 4;
    private const int LabelHeight = 28;

    private Win32.POINT[]? _positions;
    private int            _fenceWidth;
    private int            _iconSize;
    private int            _iconSpacing;
    private int            _titleBarHeight;
    private bool           _valid;

    // ── Public API ──────────────────────────────────────────────────────────

    /// <summary>Mark the layout as stale.  The next <see cref="GetPositions"/> call will recompute.</summary>
    public void Invalidate()
    {
        _valid = false;
        _positions = null;
    }

    /// <summary>
    /// Return the pre-computed icon top-left positions for <paramref name="items"/>.
    /// The array is indexed in the same order as <paramref name="items"/> and is
    /// recomputed on the first call after <see cref="Invalidate"/> (or a width change).
    /// </summary>
    public Win32.POINT[] GetPositions(IReadOnlyList<FenceItemModel> items, int fenceWidth, int iconSize, int iconSpacing, int titleBarHeight)
    {
        if (_valid
            && _fenceWidth == fenceWidth
            && _iconSize == iconSize
            && _iconSpacing == iconSpacing
            && _titleBarHeight == titleBarHeight
            && _positions?.Length == items.Count)
            return _positions;

        _positions  = Compute(items, fenceWidth, iconSize, iconSpacing, titleBarHeight);
        _fenceWidth = fenceWidth;
        _iconSize   = iconSize;
        _iconSpacing = iconSpacing;
        _titleBarHeight = titleBarHeight;
        _valid      = true;
        return _positions;
    }

    // ── Layout computation ──────────────────────────────────────────────────

    private static Win32.POINT[] Compute(IReadOnlyList<FenceItemModel> items, int fenceWidth, int iconSize, int iconSpacing, int titleBarHeight)
    {
        int verticalGap = Math.Max(iconSpacing / 2, PadY);
        int contentPadY = titleBarHeight + verticalGap;
        int cellW    = GetCellWidth(iconSize, iconSpacing);
        int cols     = Math.Max(1, (fenceWidth - PadX) / cellW);
        int cellH    = GetCellHeight(iconSize, iconSpacing);
        bool useStoredGrid = HasStableStoredGrid(items);

        var pts = new Win32.POINT[items.Count];
        for (int i = 0; i < items.Count; i++)
        {
            int col;
            int row;
            if (useStoredGrid)
            {
                col = Math.Max(0, items[i].GridColumn);
                row = Math.Max(0, items[i].GridRow);
            }
            else
            {
                col = i % cols;
                row = i / cols;
            }

            pts[i] = new Win32.POINT
            {
                x = PadX + col * cellW,
                y = contentPadY + row * cellH,
            };
        }
        return pts;
    }

    private static bool HasStableStoredGrid(IReadOnlyList<FenceItemModel> items)
    {
        if (items.Count < 2)
            return false;

        var unique = new HashSet<(int c, int r)>();
        foreach (var item in items)
            unique.Add((item.GridColumn, item.GridRow));

        // Require enough unique coordinates to treat stored layout as meaningful.
        return unique.Count >= Math.Max(2, items.Count / 2);
    }

    /// <summary>
    /// Return the index of the item at fence-local coordinates (lx, ly), or -1.
    /// Used for left-click / drag hit-testing without scanning items in WM_LBUTTONDOWN.
    /// </summary>
    public int HitTestItem(int lx, int ly, int itemCount, int fenceWidth, int fenceHeight, int iconSize, int iconSpacing, int scrollOffset, int titleBarHeight)
    {
        int cellH    = GetCellHeight(iconSize, iconSpacing);
        int cellW    = GetCellWidth(iconSize, iconSpacing);

        if (_positions == null || !_valid) return -1;
        if (ly <= titleBarHeight || ly >= fenceHeight) return -1;

        for (int i = 0; i < Math.Min(itemCount, _positions.Length); i++)
        {
            Win32.POINT p = _positions[i];
            int drawY = p.y - scrollOffset;
            if (lx >= p.x && lx < p.x + cellW &&
                ly >= drawY && ly < drawY + cellH)
                return i;
        }
        return -1;
    }

    public int GetContentHeight(int itemCount, int fenceWidth, int iconSize, int iconSpacing, int titleBarHeight)
    {
        int verticalGap = Math.Max(iconSpacing / 2, PadY);
        int contentPadY = titleBarHeight + verticalGap;
        if (itemCount <= 0)
            return contentPadY;

        int cellW = GetCellWidth(iconSize, iconSpacing);
        int cellH = GetCellHeight(iconSize, iconSpacing);
        int cols = Math.Max(1, (fenceWidth - PadX) / cellW);
        int rows = (int)Math.Ceiling(itemCount / (double)cols);
        return contentPadY + rows * cellH + PadY;
    }

    private static int GetCellWidth(int iconSize, int iconSpacing)
    {
        int spacing = Math.Clamp(iconSpacing, 2, 20);
        return iconSize + spacing * 2;
    }

    private static int GetCellHeight(int iconSize, int iconSpacing)
    {
        int spacing = Math.Clamp(iconSpacing, 2, 20);
        return iconSize + LabelHeight + Math.Max(spacing / 2, 0);
    }
}
