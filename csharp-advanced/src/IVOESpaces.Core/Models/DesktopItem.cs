using System.IO;

namespace IVOESpaces.Core.Models;

public enum DesktopItemOwnership
{
	DesktopOnly = 0,
	SpaceManaged = 1,
	PortalManaged = 2,
	PinnedOverlay = 3,
}

public record DesktopItem
{
	public Guid Id { get; init; } = Guid.NewGuid();
	public string DisplayName { get; set; } = string.Empty;
	public string ParsingName { get; set; } = string.Empty;
	public string? FileSystemPath { get; set; }
	public bool IsFolder { get; set; }
	public bool IsShortcut { get; set; }
	public string? Extension { get; set; }
	public DesktopItemOwnership Ownership { get; set; } = DesktopItemOwnership.DesktopOnly;
	public Guid? OwnerSpaceId { get; set; }

	// Backward-compatible constructor for older path-only call sites.
	public DesktopItem(string displayName, string targetPath)
	{
		DisplayName = displayName;
		ParsingName = targetPath;
		FileSystemPath = targetPath;
		IsFolder = Directory.Exists(targetPath);
		IsShortcut = string.Equals(Path.GetExtension(targetPath), ".lnk", StringComparison.OrdinalIgnoreCase);
		Extension = IsFolder ? null : Path.GetExtension(targetPath);
	}

	public DesktopItem()
	{
	}
}
