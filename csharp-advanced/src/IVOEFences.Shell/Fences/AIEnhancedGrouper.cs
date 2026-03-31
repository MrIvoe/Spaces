namespace IVOEFences.Shell.Fences;

internal sealed class AIEnhancedGrouper
{
    public Dictionary<string, List<DesktopIcon>> GroupIconsByAI(List<DesktopIcon> icons)
    {
        var groups = new Dictionary<string, List<DesktopIcon>>(StringComparer.OrdinalIgnoreCase);

        foreach (DesktopIcon icon in icons)
        {
            string category = CategorizeByAI(icon);
            if (!groups.TryGetValue(category, out List<DesktopIcon>? list))
            {
                list = new List<DesktopIcon>();
                groups[category] = list;
            }

            list.Add(icon);
        }

        return groups;
    }

    private static string CategorizeByAI(DesktopIcon icon)
    {
        string name = icon.Name.ToLowerInvariant();
        string ext = icon.Extension;

        if (icon.IsDirectory)
            return "Projects";

        if (name.Contains("report") || name.Contains("doc") ||
            ext is ".doc" or ".docx" or ".pdf" or ".txt" or ".rtf" or ".xlsx" or ".pptx")
            return "Documents";

        if (name.Contains("img") || name.Contains("photo") ||
            ext is ".png" or ".jpg" or ".jpeg" or ".gif" or ".bmp" or ".webp")
            return "Images";

        if (name.Contains("proj") || name.Contains("code") ||
            ext is ".sln" or ".csproj" or ".cs" or ".py" or ".js" or ".ts")
            return "Projects";

        if (ext is ".exe" or ".lnk" or ".url")
            return "Apps";

        return "Misc";
    }
}
