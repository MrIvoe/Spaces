using System.IO;

namespace IVOEFences.Shell.Fences;

internal sealed record DesktopIcon
{
    public string Name { get; init; } = string.Empty;
    public string FilePath { get; init; } = string.Empty;
    public bool IsDirectory { get; init; }
    public string Extension { get; init; } = string.Empty;
}

internal sealed class IconScanner
{
    public List<DesktopIcon> ScanDesktop()
    {
        string userDesktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        string commonDesktop = Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory);

        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (Directory.Exists(userDesktop))
            paths.Add(userDesktop);
        if (Directory.Exists(commonDesktop))
            paths.Add(commonDesktop);

        var icons = new List<DesktopIcon>();

        foreach (string root in paths)
        {
            foreach (string dir in Directory.EnumerateDirectories(root))
            {
                icons.Add(new DesktopIcon
                {
                    Name = Path.GetFileName(dir),
                    FilePath = dir,
                    IsDirectory = true,
                    Extension = "<DIR>"
                });
            }

            foreach (string file in Directory.EnumerateFiles(root))
            {
                icons.Add(new DesktopIcon
                {
                    Name = Path.GetFileNameWithoutExtension(file),
                    FilePath = file,
                    IsDirectory = false,
                    Extension = Path.GetExtension(file).ToLowerInvariant()
                });
            }
        }

        return icons;
    }
}
