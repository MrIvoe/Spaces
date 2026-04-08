using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media.Imaging;
using IVOESpaces.Shell;

namespace IVOESpaces.Core.Services;

/// <summary>
/// LRU icon cache using SHGetFileInfo.
/// Extracts and caches icons for desktop items and files.
/// </summary>
public sealed class IconCacheService
{
    private static readonly Lazy<IconCacheService> _instance = new(() => new IconCacheService());
    public static IconCacheService Instance => _instance.Value;

    private readonly Dictionary<(string path, int size), CacheEntry> _cache = new();
    private const int MaxEntries = 256;

    private class CacheEntry
    {
        public BitmapSource? Image { get; set; }
        public DateTime LastAccess { get; set; }
    }

    private IconCacheService() { }

    /// <summary>
    /// Gets an icon for a file path, using cache if available.
    /// LRU eviction when cache exceeds MaxEntries.
    /// </summary>
    public BitmapSource? GetIcon(string targetPath, int sizePx = 32)
    {
        if (string.IsNullOrEmpty(targetPath))
            return null;

        var key = (targetPath, sizePx);

        try
        {
            if (_cache.TryGetValue(key, out var entry))
            {
                entry.LastAccess = DateTime.UtcNow;
                return entry.Image;
            }

            // Extract and cache
            var image = ExtractIconInternal(targetPath, sizePx);
            _cache[key] = new CacheEntry { Image = image, LastAccess = DateTime.UtcNow };

            // LRU eviction
            if (_cache.Count > MaxEntries)
            {
                var oldest = _cache
                    .OrderBy(kvp => kvp.Value.LastAccess)
                    .First();
                _cache.Remove(oldest.Key);
            }

            return image;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Failed to extract icon for {Path}", targetPath);
            return null;
        }
    }

    /// <summary>
    /// Invalidates all cache entries for a specific path.
    /// </summary>
    public void InvalidatePath(string targetPath)
    {
        var keys = _cache.Keys
            .Where(k => string.Equals(k.path, targetPath, StringComparison.OrdinalIgnoreCase))
            .ToList();

        foreach (var key in keys)
            _cache.Remove(key);

        Serilog.Log.Debug("Invalidated {Count} cache entries for {Path}", keys.Count, targetPath);
    }

    /// <summary>
    /// Clears the entire cache.
    /// </summary>
    public void Clear()
    {
        _cache.Clear();
        Serilog.Log.Information("Icon cache cleared");
    }

    /// <summary>
    /// Gets cache statistics.
    /// </summary>
    public (int TotalEntries, int MaxEntries) GetStatistics()
    {
        return (_cache.Count, MaxEntries);
    }

    // ── INTERNAL ICON EXTRACTION ──

    private BitmapSource? ExtractIconInternal(string path, int sizePx)
    {
        try
        {
            // Use large icon list for larger sizes
            int imageListId = sizePx switch
            {
                > 64 => 4,  // SHIL_JUMBO (256x256)
                > 32 => 2,  // SHIL_EXTRALARGE (48x48)
                _ => 0      // SHIL_LARGE (32x32)
            };

            var fileInfo = new NativeMethods.SHFILEINFO();
            NativeMethods.SHGetFileInfo(path, 0, ref fileInfo, Marshal.SizeOf(fileInfo),
                NativeMethods.SHGFI_SYSICONINDEX | NativeMethods.SHGFI_USEFILEATTRIBUTES);

            // Try to get from image list
            var iid = NativeMethods.IID_IImageList;  // Local copy to pass by ref
            int result = NativeMethods.SHGetImageList(imageListId, ref iid, out var imgList);
            if (result == 0 && imgList != null)
            {
                try
                {
                    IntPtr hIcon = ((NativeMethods.IImageList)imgList).GetIcon(fileInfo.iIcon, NativeMethods.ILD_TRANSPARENT);
                    if (hIcon != IntPtr.Zero)
                    {
                        var bitmap = Imaging.CreateBitmapSourceFromHIcon(
                            hIcon, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
                        NativeMethods.DestroyIcon(hIcon);
                        return bitmap;
                    }
                }
                catch { }
            }

            // Fallback: standard icon extraction
            fileInfo = new NativeMethods.SHFILEINFO();
            NativeMethods.SHGetFileInfo(path, 0, ref fileInfo, Marshal.SizeOf(fileInfo),
                NativeMethods.SHGFI_ICON | NativeMethods.SHGFI_LARGEICON | NativeMethods.SHGFI_USEFILEATTRIBUTES);

            if (fileInfo.hIcon != IntPtr.Zero)
            {
                try
                {
                    var bitmap = Imaging.CreateBitmapSourceFromHIcon(
                        fileInfo.hIcon, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
                    NativeMethods.DestroyIcon(fileInfo.hIcon);
                    return bitmap;
                }
                catch { }
            }

            return null;
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Icon extraction failed for {Path}", path);
            return null;
        }
    }
}
