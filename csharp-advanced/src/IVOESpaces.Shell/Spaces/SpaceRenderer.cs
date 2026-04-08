using System.Collections.Generic;
using System.Runtime.InteropServices;
using IVOESpaces.Core.Models;
using IVOESpaces.Core.Services;
using IVOESpaces.Shell.Native;
using IVOESpaces.Shell.UI;

namespace IVOESpaces.Shell.Spaces;

/// <summary>
/// Full GDI rendering for a space window — no WPF, no XAML.
/// GDI pens, brushes, and fonts are cached per theme (dark/light) and
/// reused across all space windows; they are only recreated when the
/// OS theme changes.  All drawing goes to an off-screen bitmap that is
/// BitBlt'd to the window DC in one operation (double buffering).
/// </summary>
internal static class SpaceRenderer
{
    private readonly record struct IconGrid(int CellWidth, int CellHeight, int Padding);

    public const int TitleBarHeight = 28;

    public static int GetScaledTitleBarHeight(IntPtr hwnd)
    {
        bool enablePerMonitorDpi = AppSettingsRepository.Instance.Current.HighDpiPerMonitorScaling;
        if (!enablePerMonitorDpi)
            return TitleBarHeight;

        uint dpi = 96;
        if (hwnd != IntPtr.Zero)
            dpi = Win32.GetDpiForWindow(hwnd);

        if (dpi == 0)
            dpi = 96;

        int scaled = (int)Math.Round(TitleBarHeight * (dpi / 96.0));
        return Math.Clamp(scaled, 20, 120);
    }

    // ── Color palette ───────────────────────────────────────────────────────

    // Dark theme
    private static uint BgDark         => Win32.RGB(28,  28,  28);
    private static uint TitleBgDark    => Win32.RGB(45,  45,  48);
    private static uint TitleTextDark  => Win32.RGB(220, 220, 220);
    private static uint BorderDark     => Win32.RGB(70,  70,  70);

    // Light theme
    private static uint BgLight        => Win32.RGB(245, 245, 245);
    private static uint TitleBgLight   => Win32.RGB(230, 230, 230);
    private static uint TitleTextLight => Win32.RGB(30,  30,  30);
    private static uint BorderLight    => Win32.RGB(180, 180, 180);

    // ── Cached GDI resources (per theme + DPI) ──────────────────────────────

    private struct GdiResources
    {
        public IntPtr BgBrush;    // background fill
        public IntPtr TitleBrush; // title bar fill
        public IntPtr BorderPen;  // separator line + rounded border (shared)
        public IntPtr TitleFont;  // Segoe UI 13 pt — space title text
        public IntPtr ItemFont;   // Segoe UI 10 pt — item label text
        public bool   ForDark;
        public uint   ForDpi;
        public bool   IsValid;
    }

    // Keyed by (isDark, dpiValue) so per-monitor HiDPI just works.
    private static readonly Dictionary<(bool dark, uint dpi), GdiResources> _resCache = new();

    /// <summary>
    /// Return valid cached resources for the requested theme and DPI,
    /// rebuilding when either changes (e.g. window moves to a 4K monitor).
    /// </summary>
    private static GdiResources AcquireResources(bool dark, uint dpi)
    {
        var key = (dark, dpi);
        if (_resCache.TryGetValue(key, out var cached) && cached.IsValid)
            return cached;

        // Scale 96-DPI baseline font heights to the actual DPI.
        int titleFontPx = (int)Math.Round(13 * dpi / 96.0);
        int itemFontPx  = (int)Math.Round(11 * dpi / 96.0);

        var res = new GdiResources
        {
            BgBrush    = Win32.CreateSolidBrush(dark ? BgDark      : BgLight),
            TitleBrush = Win32.CreateSolidBrush(dark ? TitleBgDark : TitleBgLight),
            BorderPen  = Win32.CreatePen(Win32.PS_SOLID, 1, dark ? BorderDark : BorderLight),
            TitleFont  = Win32.CreateFont(
                titleFontPx, 0, 0, 0, Win32.FW_NORMAL, 0, 0, 0,
                (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
                (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
                (uint)Win32.DEFAULT_PITCH, "Segoe UI"),
            ItemFont   = Win32.CreateFont(
                itemFontPx, 0, 0, 0, Win32.FW_NORMAL, 0, 0, 0,
                (uint)Win32.DEFAULT_CHARSET, (uint)Win32.OUT_DEFAULT_PRECIS,
                (uint)Win32.CLIP_DEFAULT_PRECIS, (uint)Win32.CLEARTYPE_QUALITY,
                (uint)Win32.DEFAULT_PITCH, "Segoe UI"),
            ForDark = dark,
            ForDpi  = dpi,
            IsValid = true,
        };
        _resCache[key] = res;
        return res;
    }

    /// <summary>
    /// Release all cached GDI objects.  Called automatically when the theme
    /// changes or on application shutdown.
    /// </summary>
    public static void ReleaseResources()
    {
        foreach (var res in _resCache.Values)
        {
            if (!res.IsValid) continue;
            Win32.DeleteObject(res.BgBrush);
            Win32.DeleteObject(res.TitleBrush);
            Win32.DeleteObject(res.BorderPen);
            Win32.DeleteObject(res.TitleFont);
            Win32.DeleteObject(res.ItemFont);
        }
        _resCache.Clear();
    }

    // ── Public entry point ──────────────────────────────────────────────────

    /// <summary>
    /// WM_PAINT handler.  Draws entirely into an off-screen bitmap, then
    /// BitBlt's the completed frame to the window DC — no partial-paint
    /// flicker.  GDI objects are reused from the per-theme cache.
    /// </summary>
    /// <param name="items">Space items to render below the title bar; null = empty.</param>
    /// <param name="rolledUp">When true the space is collapsed — skip item rendering.</param>
    /// <param name="positions">
    /// Pre-computed icon top-left positions from SpaceIconLayout.
    /// When null the renderer falls back to inline calculation (slower path).
    /// </param>
    /// <param name="searchQuery">Current search filter query (may be empty).</param>
    /// <param name="tabManager">Tab manager for rendering tab UI (may be null).</param>
    public static void Paint(IntPtr hwnd, string title,
        IReadOnlyList<SpaceItemModel>? items = null, bool rolledUp = false,
        Win32.POINT[]? positions = null, int iconSize = 48, string? searchQuery = null,
        SpaceTabManager? tabManager = null, bool aiSuggested = false,
        bool showTitleBar = true, string? backgroundColorOverride = null, string? titleTextColorOverride = null,
        int scrollOffset = 0, bool snapFlash = false, bool isLocked = false,
        Guid? selectedItemId = null, Guid? hoveredItemId = null,
        bool isPortal = false, string portalViewMode = "Icons", int iconSpacing = 8)
    {
        int titleBarHeight = GetScaledTitleBarHeight(hwnd);
        uint dpi = hwnd != IntPtr.Zero ? Win32.GetDpiForWindow(hwnd) : 96u;
        if (dpi == 0) dpi = 96u;

        IntPtr hdc = Win32.BeginPaint(hwnd, out Win32.PAINTSTRUCT ps);
        try
        {
            Win32.GetClientRect(hwnd, out Win32.RECT client);
            int w = client.right  - client.left;
            int h = client.bottom - client.top;

            // Off-screen bitmap - prevents flicker
            IntPtr hdcMem    = Win32.CreateCompatibleDC(hdc);
            IntPtr hBitmap   = Win32.CreateCompatibleBitmap(hdc, w, h);
            IntPtr oldBitmap = Win32.SelectObject(hdcMem, hBitmap);
            try
            {
                DrawFrame(hdcMem, client, title, items, rolledUp, positions, iconSize, searchQuery, tabManager, aiSuggested,
                    showTitleBar, backgroundColorOverride, titleTextColorOverride, scrollOffset, snapFlash, isLocked,
                    selectedItemId, hoveredItemId,
                    isPortal, portalViewMode, titleBarHeight, iconSpacing, dpi);
                // One atomic copy to the window surface
                Win32.BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, Win32.SRCCOPY);
            }
            finally
            {
                Win32.SelectObject(hdcMem, oldBitmap);
                Win32.DeleteObject(hBitmap);
                Win32.DeleteDC(hdcMem);
            }
        }
        finally
        {
            Win32.EndPaint(hwnd, ref ps);
        }
    }

    // ── Frame drawing (into off-screen DC) ─────────────────────────────────
    private static void DrawFrame(IntPtr hdc, Win32.RECT client, string title,
        IReadOnlyList<SpaceItemModel>? items, bool rolledUp, Win32.POINT[]? positions, int iconSize,
        string? searchQuery = null, SpaceTabManager? tabManager = null, bool aiSuggested = false,
        bool showTitleBar = true, string? backgroundColorOverride = null, string? titleTextColorOverride = null,
        int scrollOffset = 0, bool snapFlash = false, bool isLocked = false,
        Guid? selectedItemId = null, Guid? hoveredItemId = null,
        bool isPortal = false, string portalViewMode = "Icons", int titleBarHeight = TitleBarHeight, int iconSpacing = 8, uint dpi = 96)
    {
        bool dark = IsDarkMode();
        var  res  = AcquireResources(dark, dpi);

        // Background
        IntPtr bgBrush = res.BgBrush;
        IntPtr customBgBrush = IntPtr.Zero;
        if (TryParseArgbHex(backgroundColorOverride, out uint customBgColor))
        {
            customBgBrush = Win32.CreateSolidBrush(customBgColor);
            bgBrush = customBgBrush;
        }

        Win32.FillRect(hdc, ref client, bgBrush);

        // Title bar
        var titleBar = new Win32.RECT
        {
            left   = client.left,
            top    = client.top,
            right  = client.right,
            bottom = client.top + titleBarHeight,
        };
        if (showTitleBar)
            Win32.FillRect(hdc, ref titleBar, res.TitleBrush);

        // Title text
        IntPtr oldFont = Win32.SelectObject(hdc, res.TitleFont);
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        uint effectiveTitleText = dark ? TitleTextDark : TitleTextLight;
        if (TryParseArgbHex(titleTextColorOverride, out uint customTitleText))
            effectiveTitleText = customTitleText;
        Win32.SetTextColor(hdc, effectiveTitleText);
        
        string displayTitle = title;
        
        // Append tab indicator if tabbed
        if (tabManager?.IsTabbed == true)
            displayTitle += $" [{tabManager.ActiveTabIndex + 1}/{tabManager.TabCount}]";
        
        var badges = new List<FluentIcon>();
        if (!string.IsNullOrWhiteSpace(searchQuery)) badges.Add(FluentIcon.Search);
        if (isPortal) badges.Add(FluentIcon.Folder);
        if (aiSuggested) badges.Add(FluentIcon.BotSparkle);
        if (isLocked) badges.Add(FluentIcon.LockClosed);

        int badgeTotalWidth = badges.Count * 22;
        
        var textRect = new Win32.RECT
        {
            left   = titleBar.left + 10,
            top    = titleBar.top,
            right  = titleBar.right - 14 - badgeTotalWidth,
            bottom = titleBar.bottom,
        };
        if (showTitleBar)
        {
            Win32.DrawText(hdc, displayTitle, -1, ref textRect,
                Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

            int badgeX = titleBar.right - 10 - badgeTotalWidth;
            foreach (FluentIcon badge in badges)
            {
                var chip = new Win32.RECT
                {
                    left = badgeX,
                    top = titleBar.top + 5,
                    right = badgeX + 18,
                    bottom = titleBar.bottom - 5,
                };
                IntPtr chipBrush = Win32.CreateSolidBrush(dark ? Win32.RGB(58, 58, 62) : Win32.RGB(222, 228, 236));
                Win32.FillRect(hdc, ref chip, chipBrush);
                Win32.DeleteObject(chipBrush);

                FluentIcons.DrawGlyph(hdc, badge, badgeX + 2, titleBar.top + 6, 12, effectiveTitleText);
                badgeX += 22;
            }
        }
        Win32.SelectObject(hdc, oldFont);

        // Separator line + rounded border (shared cached pen)
        IntPtr oldPen   = Win32.SelectObject(hdc, res.BorderPen);
        if (showTitleBar)
        {
            Win32.MoveToEx(hdc, client.left,  titleBarHeight, IntPtr.Zero);
            Win32.LineTo  (hdc, client.right, titleBarHeight);
        }

        IntPtr nullBrush = Win32.GetStockObject(Win32.NULL_BRUSH); // never deleted
        IntPtr oldBrush  = Win32.SelectObject(hdc, nullBrush);
        Win32.RoundRect(hdc,
            client.left, client.top,
            client.right - 1, client.bottom - 1,
            8, 8);

        // Subtle AI highlight border for suggested spaces.
        if (aiSuggested)
        {
            IntPtr aiPen = Win32.CreatePen(Win32.PS_SOLID, 2, Win32.RGB(100, 149, 237));
            IntPtr oldAiPen = Win32.SelectObject(hdc, aiPen);
            Win32.RoundRect(hdc,
                client.left + 1, client.top + 1,
                client.right - 2, client.bottom - 2,
                8, 8);
            Win32.SelectObject(hdc, oldAiPen);
            Win32.DeleteObject(aiPen);
        }

        // Snap-to-edge flash: blue border for 200 ms when a snap occurs during drag.
        if (snapFlash)
        {
            IntPtr snapPen = Win32.CreatePen(Win32.PS_SOLID, 2, Win32.RGB(0, 120, 215));
            IntPtr oldSnapPen = Win32.SelectObject(hdc, snapPen);
            Win32.RoundRect(hdc,
                client.left + 1, client.top + 1,
                client.right - 2, client.bottom - 2,
                8, 8);
            Win32.SelectObject(hdc, oldSnapPen);
            Win32.DeleteObject(snapPen);
        }

        Win32.SelectObject(hdc, oldBrush);
        Win32.SelectObject(hdc, oldPen);

        // Item grid (skipped when space is rolled up)
        if (!rolledUp && items?.Count > 0)
            DrawItems(hdc, client, items, dark, res, positions, iconSize, scrollOffset, isPortal, portalViewMode, titleBarHeight, iconSpacing, dpi, selectedItemId, hoveredItemId);

        if (customBgBrush != IntPtr.Zero)
            Win32.DeleteObject(customBgBrush);
    }

    private static bool TryParseArgbHex(string? value, out uint colorRef)
    {
        colorRef = 0;
        if (string.IsNullOrWhiteSpace(value))
            return false;

        string hex = value.Trim();
        if (hex.StartsWith("#", StringComparison.Ordinal))
            hex = hex.Substring(1);

        if (hex.Length == 8)
            hex = hex.Substring(2); // ignore alpha channel for COLORREF

        if (hex.Length != 6)
            return false;

        if (!uint.TryParse(hex, System.Globalization.NumberStyles.HexNumber,
            System.Globalization.CultureInfo.InvariantCulture, out uint rgb))
            return false;

        byte r = (byte)((rgb >> 16) & 0xFF);
        byte g = (byte)((rgb >> 8) & 0xFF);
        byte b = (byte)(rgb & 0xFF);
        colorRef = Win32.RGB(r, g, b);
        return true;
    }

    // ── Item rendering ───────────────────────────────────────────────────────

    // HICON cache: key = "<path>@<size>", value = HICON from SHGetFileInfo.
    // Populated lazily on first paint; cleared at app exit via ReleaseIconCache().
    private static readonly Dictionary<string, IntPtr> _iconCache = new();

    /// <summary>
    /// Draw space items as an icon grid below the title bar.
    /// When <paramref name="positions"/> is provided (pre-computed by
    /// <see cref="SpaceIconLayout"/>) the renderer skips its own layout
    /// calculation.  The fallback inline layout is used only when positions
    /// is null (e.g. during the very first paint after window creation).
    /// </summary>
    /// <summary>
    /// Draw space items as an icon grid below the title bar.
    /// When positions is provided (pre-computed by SpaceIconLayout) the renderer skips
    /// its own layout calculation. The fallback inline layout is used only when positions
    /// is null (e.g. during the very first paint after window creation).
    /// </summary>
    private static void DrawItems(
        IntPtr hdc, Win32.RECT client,
        IReadOnlyList<SpaceItemModel> items, bool dark, GdiResources res,
        Win32.POINT[]? positions, int iconSize, int scrollOffset, bool isPortal, string portalViewMode, int titleBarHeight, int iconSpacing, uint dpi,
        Guid? selectedItemId, Guid? hoveredItemId)
    {
        if (isPortal && string.Equals(portalViewMode, "List", StringComparison.OrdinalIgnoreCase))
        {
            DrawPortalList(hdc, client, items, dark, res, iconSize, scrollOffset, detailsMode: false, titleBarHeight, dpi);
            return;
        }

        if (isPortal && string.Equals(portalViewMode, "Details", StringComparison.OrdinalIgnoreCase))
        {
            DrawPortalList(hdc, client, items, dark, res, iconSize, scrollOffset, detailsMode: true, titleBarHeight, dpi);
            return;
        }

        int drawIconSize = ScaleForDpi(iconSize, dpi, 16, 256);
        int spacing = Math.Clamp(iconSpacing, 2, 20);
        int cellW    = drawIconSize + spacing * 2;
        int cellH    = drawIconSize + 28 + Math.Max(spacing / 2, 0);
        int PadX = Math.Max(spacing, 2);
        int PadY = Math.Max(spacing / 2, 2);
        int contentW = client.right - client.left;
        int cols     = Math.Max(1, (contentW - PadX) / (drawIconSize + PadX * 2));
        var grid = new IconGrid(cellW, cellH, PadX);

        IntPtr oldFont = Win32.SelectObject(hdc, res.ItemFont);
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, dark ? Win32.RGB(210, 210, 210) : Win32.RGB(30, 30, 30));

        for (int i = 0; i < items.Count; i++)
        {
            SpaceItemModel item = items[i];

            // Use pre-computed position when available, else fall back to inline
            int cx, cy;
            if (positions != null && i < positions.Length)
            {
                cx = client.left + positions[i].x;
                cy = positions[i].y - scrollOffset;
            }
            else
            {
                int col = i % cols;
                int row = i / cols;
                Win32.POINT p = GetGridPosition(col, row, grid);
                cx = client.left + p.x;
                cy = titleBarHeight + PadY + p.y - scrollOffset;
            }

            if (cy + cellH <= titleBarHeight)
                continue;   // fully above the visible title bar — skip everything, including icon load

            if (cy >= client.bottom)
                break;      // past the bottom of the space — no more visible items

            // Icon
            IntPtr hIcon = GetOrLoadIcon(item.TargetPath, drawIconSize);
            if (hIcon != IntPtr.Zero)
                Win32.DrawIconEx(hdc,
                    cx + (cellW - drawIconSize) / 2, cy + 2,
                    hIcon, drawIconSize, drawIconSize, 0, IntPtr.Zero, Win32.DI_NORMAL);

            bool selected = selectedItemId.HasValue && selectedItemId.Value == item.Id;
            bool hovered = hoveredItemId.HasValue && hoveredItemId.Value == item.Id;
            if (selected || hovered)
            {
                uint accent = selected
                    ? (dark ? Win32.RGB(51, 102, 184) : Win32.RGB(173, 214, 255))
                    : (dark ? Win32.RGB(60, 60, 60) : Win32.RGB(230, 240, 252));

                var highlightRect = new Win32.RECT
                {
                    left = cx + 2,
                    top = cy + 1,
                    right = cx + cellW - 2,
                    bottom = cy + cellH - 2,
                };

                IntPtr highlightBrush = Win32.CreateSolidBrush(accent);
                Win32.FrameRect(hdc, ref highlightRect, highlightBrush);
                Win32.DeleteObject(highlightBrush);
            }

            // Label
            string label     = TruncateLabel(item.DisplayName, 14);
            var    labelRect = new Win32.RECT
            {
                left   = cx,
                top    = cy + drawIconSize + 4,
                right  = cx + cellW,
                bottom = cy + cellH,
            };
            Win32.DrawText(hdc, label, -1, ref labelRect,
                Win32.DT_CENTER | Win32.DT_WORDBREAK | Win32.DT_END_ELLIPSIS | Win32.DT_NOPREFIX);
        }

        Win32.SelectObject(hdc, oldFont);
    }

    private static void DrawPortalList(
        IntPtr hdc,
        Win32.RECT client,
        IReadOnlyList<SpaceItemModel> items,
        bool dark,
        GdiResources res,
        int iconSize,
        int scrollOffset,
        bool detailsMode,
        int titleBarHeight,
        uint dpi)
    {
        int top = titleBarHeight + 4;
        int rowH = detailsMode ? 24 : 22;
        int iconDraw = Math.Clamp(ScaleForDpi(Math.Max(16, Math.Min(iconSize, 20)), dpi, 16, 32), 16, 32);
        int y = top - scrollOffset;

        IntPtr oldFont = Win32.SelectObject(hdc, res.ItemFont);
        Win32.SetBkMode(hdc, Win32.TRANSPARENT);
        Win32.SetTextColor(hdc, dark ? TitleTextDark : TitleTextLight);

        if (detailsMode)
        {
            var header = new Win32.RECT { left = 8, top = y, right = client.right - 8, bottom = y + rowH };
            Win32.DrawText(hdc, "Name", -1, ref header, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

            var dateHeader = new Win32.RECT { left = client.right - 190, top = y, right = client.right - 90, bottom = y + rowH };
            Win32.DrawText(hdc, "Modified", -1, ref dateHeader, Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);

            var sizeHeader = new Win32.RECT { left = client.right - 84, top = y, right = client.right - 8, bottom = y + rowH };
            Win32.DrawText(hdc, "Size", -1, ref sizeHeader, Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE);
            y += rowH;
        }

        foreach (var item in items)
        {
            var row = new Win32.RECT { left = 8, top = y, right = client.right - 8, bottom = y + rowH };
            if (row.bottom < top)
            {
                y += rowH;
                continue;
            }

            if (row.top > client.bottom)
                break;

            IntPtr hIcon = GetOrLoadIcon(item.TargetPath, iconDraw);
            if (hIcon != IntPtr.Zero)
            {
                int iy = row.top + Math.Max(0, (rowH - iconDraw) / 2);
                Win32.DrawIconEx(hdc, row.left, iy, hIcon, iconDraw, iconDraw, 0, IntPtr.Zero, Win32.DI_NORMAL);
            }

            var nameRect = new Win32.RECT
            {
                left = row.left + iconDraw + 8,
                top = row.top,
                right = detailsMode ? client.right - 196 : row.right,
                bottom = row.bottom,
            };
            Win32.DrawText(hdc, item.DisplayName, -1, ref nameRect,
                Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

            if (detailsMode)
            {
                string modified = "-";
                string sizeText = item.IsDirectory ? "<DIR>" : "-";

                try
                {
                    if (!string.IsNullOrWhiteSpace(item.TargetPath) && File.Exists(item.TargetPath))
                    {
                        var fi = new FileInfo(item.TargetPath);
                        modified = fi.LastWriteTime.ToString("yyyy-MM-dd");
                        sizeText = FormatFileSize(fi.Length);
                    }
                    else if (!string.IsNullOrWhiteSpace(item.TargetPath) && Directory.Exists(item.TargetPath))
                    {
                        modified = Directory.GetLastWriteTime(item.TargetPath).ToString("yyyy-MM-dd");
                    }
                }
                catch
                {
                    // Keep defaults for unavailable paths.
                }

                var dateRect = new Win32.RECT { left = client.right - 190, top = row.top, right = client.right - 90, bottom = row.bottom };
                Win32.DrawText(hdc, modified, -1, ref dateRect,
                    Win32.DT_LEFT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);

                var sizeRect = new Win32.RECT { left = client.right - 84, top = row.top, right = client.right - 8, bottom = row.bottom };
                Win32.DrawText(hdc, sizeText, -1, ref sizeRect,
                    Win32.DT_RIGHT | Win32.DT_VCENTER | Win32.DT_SINGLELINE | Win32.DT_END_ELLIPSIS);
            }

            y += rowH;
        }

        Win32.SelectObject(hdc, oldFont);
    }

    private static Win32.POINT GetGridPosition(int col, int row, IconGrid grid)
    {
        return new Win32.POINT
        {
            x = grid.Padding + col * grid.CellWidth,
            y = row * grid.CellHeight,
        };
    }

    private static string FormatFileSize(long bytes)
    {
        if (bytes < 1024) return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:0.#} KB";
        if (bytes < 1024L * 1024L * 1024L) return $"{bytes / (1024.0 * 1024.0):0.#} MB";
        return $"{bytes / (1024.0 * 1024.0 * 1024.0):0.#} GB";
    }

    private static IntPtr GetOrLoadIcon(string targetPath, int size)
    {
        if (string.IsNullOrEmpty(targetPath)) return IntPtr.Zero;

        string cacheKey = $"{targetPath}@{size}";
        if (_iconCache.TryGetValue(cacheKey, out IntPtr cached)) return cached;

        IntPtr hIcon = TryExtractSizedIcon(targetPath, size);
        if (hIcon == IntPtr.Zero)
        {
            var  shfi  = new Shell32.SHFILEINFO();
            uint flags = Shell32.SHGFI_ICON |
                         (size <= 16 ? Shell32.SHGFI_SMALLICON : Shell32.SHGFI_LARGEICON);
            IntPtr r = Shell32.SHGetFileInfo(
                targetPath, 0, ref shfi,
                (uint)Marshal.SizeOf<Shell32.SHFILEINFO>(), flags);
            hIcon = r != IntPtr.Zero ? shfi.hIcon : IntPtr.Zero;
        }

        _iconCache[cacheKey] = hIcon;
        return hIcon;
    }

    private static IntPtr TryExtractSizedIcon(string targetPath, int size)
    {
        IntPtr shellIcon = Shell32.TryGetHighQualityIcon(targetPath, size);
        if (shellIcon != IntPtr.Zero)
            return shellIcon;

        try
        {
            var icons = new IntPtr[1];
            var ids = new uint[1];
            uint extracted = Shell32.PrivateExtractIcons(
                targetPath,
                0,
                size,
                size,
                icons,
                ids,
                1,
                0);

            if (extracted > 0 && icons[0] != IntPtr.Zero)
                return icons[0];
        }
        catch
        {
            // Fall back to SHGetFileInfo for unsupported types.
        }

        return IntPtr.Zero;
    }

    private static string TruncateLabel(string text, int maxChars) =>
        text.Length <= maxChars ? text : text[..(maxChars - 1)] + "\u2026";

    private static int ScaleForDpi(int logicalPixels, uint dpi, int min, int max)
    {
        if (dpi == 0)
            dpi = 96;

        int scaled = (int)Math.Round(logicalPixels * (dpi / 96.0));
        return Math.Clamp(scaled, min, max);
    }

    /// <summary>Destroy all cached HICON handles. Call once on application exit.</summary>
    public static void ReleaseIconCache()
    {
        foreach (IntPtr hIcon in _iconCache.Values)
            if (hIcon != IntPtr.Zero) Shell32.DestroyIcon(hIcon);
        _iconCache.Clear();
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    /// <summary>Detect Windows dark/light app mode from the registry.</summary>
    private static bool IsDarkMode()
    {
        try
        {
            using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            return (int)(key?.GetValue("AppsUseLightTheme") ?? 1) == 0;
        }
        catch
        {
            return false;
        }
    }
}
