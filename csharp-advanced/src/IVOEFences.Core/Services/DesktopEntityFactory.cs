using IVOEFences.Core.Models;

namespace IVOEFences.Core.Services;

public static class DesktopEntityFactory
{
    public static DesktopEntity? FromPath(string path)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(path))
                return null;

            bool isDirectory = Directory.Exists(path);
            bool isFile = File.Exists(path);
            if (!isDirectory && !isFile)
                return null;

            string ext = isDirectory ? "<DIR>" : Path.GetExtension(path).ToLowerInvariant();

            return new DesktopEntity
            {
                DisplayName = isDirectory
                    ? Path.GetFileName(path)
                    : Path.GetFileNameWithoutExtension(path),
                ParsingPath = path,
                FileSystemPath = path,
                Kind = isDirectory
                    ? DesktopEntityKind.Directory
                    : ext == ".lnk" ? DesktopEntityKind.Shortcut : DesktopEntityKind.File,
                Extension = ext,
                IsMissing = false,
                LastSeenUtc = DateTime.UtcNow,
            };
        }
        catch
        {
            return null;
        }
    }
}
