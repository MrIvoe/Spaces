namespace IVOEFences.Shell.Fences;

internal sealed class AutoGrouper
{
    public Dictionary<string, List<DesktopIcon>> GroupIcons(List<DesktopIcon> icons)
    {
        var groups = new Dictionary<string, List<DesktopIcon>>(StringComparer.OrdinalIgnoreCase);

        foreach (DesktopIcon icon in icons)
        {
            string key = NormalizeKey(icon.Extension, icon.IsDirectory);
            if (!groups.TryGetValue(key, out List<DesktopIcon>? list))
            {
                list = new List<DesktopIcon>();
                groups[key] = list;
            }

            list.Add(icon);
        }

        return groups;
    }

    private static string NormalizeKey(string extension, bool isDirectory)
    {
        if (isDirectory)
            return "Folders";

        return extension switch
        {
            ".lnk" => "Shortcuts",
            ".exe" => "Applications",
            ".url" => "Links",
            _ => string.IsNullOrWhiteSpace(extension) ? "Other" : extension.ToUpperInvariant()
        };
    }
}
