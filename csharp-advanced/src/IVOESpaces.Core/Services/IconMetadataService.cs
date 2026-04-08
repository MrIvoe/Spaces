using IVOESpaces.Core.Models;

namespace IVOESpaces.Core.Services;

public sealed class IconMetadataService
{
    private static readonly Lazy<IconMetadataService> _instance = new(() => new IconMetadataService());
    public static IconMetadataService Instance => _instance.Value;

    private static readonly Dictionary<string, string[]> TagMap = new(StringComparer.OrdinalIgnoreCase)
    {
        [".exe"] = new[] { "app", "executable" },
        [".lnk"] = new[] { "shortcut" },
        [".docx"] = new[] { "document", "work" },
        [".xlsx"] = new[] { "spreadsheet", "work" },
        [".pdf"] = new[] { "document" },
        [".png"] = new[] { "image", "media" },
        [".jpg"] = new[] { "image", "media" },
        [".mp4"] = new[] { "video", "media" },
        [".zip"] = new[] { "archive" }
    };

    private IconMetadataService()
    {
    }

    public void AutoTag(SpaceItemModel item)
    {
        if (item == null)
            return;

        string? ext = SpaceItemResolver.Instance.GetExtension(item);
        if (string.IsNullOrWhiteSpace(ext))
        {
            // If no extension cached yet, cache metadata first
            SpaceUsageTracker.Instance.CacheFileMetadata(item);
            ext = SpaceItemResolver.Instance.GetExtension(item);
        }

        if (string.IsNullOrWhiteSpace(ext) || !TagMap.TryGetValue(ext, out string[]? tags))
            return;

        foreach (string tag in tags)
        {
            if (!item.Tags.Contains(tag, StringComparer.OrdinalIgnoreCase))
                item.Tags.Add(tag);
        }
    }

    public void AutoTagSpace(SpaceModel space)
    {
        foreach (SpaceItemModel item in space.Items)
            AutoTag(item);
    }
}
